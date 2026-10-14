/* Linux x86 port I/O belongs exclusively to this worker, including ioperm.
   ALSA/SDL PCM stays on the host thread; no sound-device calls run here. */
#define _POSIX_C_SOURCE 200809L
#include "WG_LINUX_OPL.h"
#include "WOLF3D.h"
#include "../WG_OPL_TIMELINE.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#ifdef WG_LINUX_OPL_LEGACY
#include <sys/time.h>
#include <sys/select.h>
/* RH9 LinuxThreads has no selectable condition-variable clock. Serialize
   wall-clock observations and fail closed on backwards/large forward jumps. */
static pthread_mutex_t wg_clock_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t wg_last_clock;
static int wg_legacy_clock_failed;
static uint64_t WG_OPLLegacyClock(uint64_t now)
{
    if (!now || (wg_last_clock &&
        (now < wg_last_clock || now - wg_last_clock > 5000000000ULL)))
        wg_legacy_clock_failed = 1;
    wg_last_clock = now;
    return wg_legacy_clock_failed ? 0 : now;
}
#endif
#ifndef WG_LINUX_OPL_TEST
#include <sys/io.h>
#endif

static pthread_mutex_t wg_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wg_wake;
static pthread_t wg_worker;
static wg_opl_timeline_t wg_timeline;
static uint32_t wg_rate;
static int wg_running, wg_ready, wg_stop, wg_failed;
static const char *wg_failure;
/* Worker-owned; published under wg_lock after probe/write. */
static int wg_clock_failed;
#ifdef WG_LINUX_OPL_LEGACY
static int wg_recovery_timing; /* Worker-only emergency chip shutdown. */
#endif

static uint64_t WG_OPLNow(void)
{
#ifdef WG_LINUX_OPL_LEGACY
    struct timeval tv;
    uint64_t now;
    pthread_mutex_lock(&wg_clock_lock);
    now = gettimeofday(&tv, NULL) == 0 ?
        (uint64_t)tv.tv_sec * 1000000000U + (uint64_t)tv.tv_usec * 1000U : 0;
    now = WG_OPLLegacyClock(now);
    pthread_mutex_unlock(&wg_clock_lock);
    return now;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000U + (uint64_t)ts.tv_nsec;
#endif
}

static void WG_OPLWait(unsigned us)
{
    uint64_t start = WG_OPLNow();
    /* Short chip recovery intervals must not use scheduler sleeps. */
    for (;;)
    {
        uint64_t now = WG_OPLNow();
        if (!start || !now || now < start) { wg_clock_failed = 1; return; }
        if (now - start >= (uint64_t)us * 1000U) return;
    }
}

static void WG_OPLPortWrite(unsigned bank, uint8_t reg, uint8_t value)
{
#ifdef WG_LINUX_OPL_LEGACY
    if (wg_recovery_timing) {
        unsigned i;
        /* If the clock failed, silence still needs safe hardware delays. */
        outb(reg, (unsigned short)(0x388U + bank));
        for (i=0;i<6;++i) (void)inb(0x388);
        outb(value, (unsigned short)(0x389U + bank));
        for (i=0;i<35;++i) (void)inb(0x388);
        return;
    }
#endif
    if (wg_clock_failed) return;
    outb(reg, (unsigned short)(0x388U + bank));
    WG_OPLWait(4);
    if (wg_clock_failed) return;
    outb(value, (unsigned short)(0x389U + bank));
    WG_OPLWait(24);
}

static void WG_OPLSilence(void)
{
    unsigned reg;
    WG_OPLPortWrite(0, 0xbd, 0);
    for (reg = 0xb0; reg <= 0xb8; ++reg) WG_OPLPortWrite(0, (uint8_t)reg, 0);
    WG_OPLPortWrite(0, 4, 0x60);
    WG_OPLPortWrite(0, 4, 0x80);
}

static int WG_OPLProbe(void)
{
    unsigned reg;
    uint8_t before, after;
    WG_OPLPortWrite(0, 4, 0x60);
    WG_OPLPortWrite(0, 4, 0x80);
    before = inb(0x388);
    WG_OPLPortWrite(0, 2, 0xff);
    WG_OPLPortWrite(0, 4, 0x21);
    WG_OPLWait(100);
    after = inb(0x388);
    WG_OPLPortWrite(0, 4, 0x60);
    WG_OPLPortWrite(0, 4, 0x80);
    if ((before & 0xe0U) || (after & 0xe0U) != 0xc0U) return 0;
    if ((before & 6U) == 0) /* OPL3: silence bank two and restore OPL2 mode. */
    {
        for (reg = 0xb0; reg <= 0xb8; ++reg) WG_OPLPortWrite(2, (uint8_t)reg, 0);
        WG_OPLPortWrite(2, 4, 0);
        WG_OPLPortWrite(2, 5, 0);
    }
    for (reg = 1; reg <= 0xf5; ++reg) WG_OPLPortWrite(0, (uint8_t)reg, 0);
    WG_OPLPortWrite(0, 1, 0x20);
    return 1;
}

static void *WG_OPLWorker(void *unused)
{
    int permission, detected = 0;
    const char *failure = NULL;
    (void)unused;
    permission = ioperm(0x388, 4, 1) == 0;
    if (!permission)
        failure = errno == EPERM ? "Linux OPL port access denied (CAP_SYS_RAWIO required; kernel security policy may restrict access)"
                                  : "Linux could not enable OPL port access at 0x388";
    else if (!(detected = WG_OPLProbe()))
        failure = "OPL chip did not respond at 0x388";
    if (wg_clock_failed) failure = "Linux OPL playback clock failed";
    pthread_mutex_lock(&wg_lock);
    wg_failure = failure;
    wg_failed = failure != NULL;
    wg_ready = 1;
    pthread_cond_broadcast(&wg_wake);
    while (!wg_stop && !wg_failed)
    {
        wg_opl_event_t event;
        uint64_t now = WG_OPLNow();
        if (!now) { wg_failed = 1; wg_failure = "Linux OPL playback clock failed"; break; }
        if (WG_OPLTimelinePop(&wg_timeline, now, &event))
        {
            pthread_mutex_unlock(&wg_lock);
            WG_OPLPortWrite(0, (uint8_t)event.reg, event.value);
            pthread_mutex_lock(&wg_lock);
            if (wg_clock_failed) { wg_failed = 1; wg_failure = "Linux OPL playback clock failed"; }
        }
        else
        {
#ifdef WG_LINUX_OPL_LEGACY
            /* Relative timeout cannot hang shutdown if wall time is set back.
               Linux 2.4 may round this to one scheduler tick. */
            struct timeval delay = {0, 1000};
            pthread_mutex_unlock(&wg_lock);
            (void)select(0, NULL, NULL, NULL, &delay);
            pthread_mutex_lock(&wg_lock);
#else
            struct timespec deadline;
            now += 1000000U; /* At most 1 ms polling; submissions wake sooner. */
            deadline.tv_sec = (time_t)(now / 1000000000U);
            deadline.tv_nsec = (long)(now % 1000000000U);
            pthread_cond_timedwait(&wg_wake, &wg_lock, &deadline);
#endif
        }
    }
    pthread_mutex_unlock(&wg_lock);
#ifdef WG_LINUX_OPL_LEGACY
    wg_recovery_timing = wg_clock_failed || !WG_OPLNow();
#endif
    if (detected) WG_OPLSilence();
    if (permission) (void)ioperm(0x388, 4, 0);
    return NULL;
}

int WG_LinuxOPLActive(void) { return wg_running; }

void WG_LinuxOPLSetRate(uint32_t rate) { wg_rate = rate; }

void WG_LinuxOPLShutdown(void)
{
    if (!wg_running) return;
    pthread_mutex_lock(&wg_lock);
    wg_stop = 1;
    pthread_cond_broadcast(&wg_wake);
    pthread_mutex_unlock(&wg_lock);
    pthread_join(wg_worker, NULL);
    pthread_cond_destroy(&wg_wake);
    wg_running = 0;
}

int WG_LinuxOPLInit(void)
{
#ifndef WG_LINUX_OPL_LEGACY
    pthread_condattr_t attr;
#endif
    int ok;
    WG_LinuxOPLShutdown();
#ifdef WG_LINUX_OPL_LEGACY
    wg_last_clock = 0;
    wg_legacy_clock_failed = 0;
    wg_recovery_timing = 0;
#endif
    if (!wg_rate || !WG_OPLNow())
    {
        wolf3d_SetOPLError("Linux OPL requires PCM and a playback clock");
        return 0;
    }
#ifdef WG_LINUX_OPL_LEGACY
    /* Used for the startup handshake; playback uses relative waits below. */
    ok = pthread_cond_init(&wg_wake, NULL) == 0;
#else
    if (pthread_condattr_init(&attr) != 0)
    { wolf3d_SetOPLError("Linux OPL worker setup failed"); return 0; }
    ok = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0;
    if (ok) ok = pthread_cond_init(&wg_wake, &attr) == 0;
    pthread_condattr_destroy(&attr);
#endif
    if (!ok) { wolf3d_SetOPLError("Linux OPL worker clock setup failed"); return 0; }
    WG_OPLTimelineInit(&wg_timeline, wg_rate, 1000000000U);
    wg_ready = wg_stop = wg_failed = wg_clock_failed = 0;
    wg_failure = NULL;
    if (pthread_create(&wg_worker, NULL, WG_OPLWorker, NULL) != 0)
    {
        pthread_cond_destroy(&wg_wake);
        wolf3d_SetOPLError("Linux OPL worker creation failed");
        return 0;
    }
    wg_running = 1;
    pthread_mutex_lock(&wg_lock);
    while (!wg_ready) pthread_cond_wait(&wg_wake, &wg_lock);
    ok = !wg_failed;
    if (!ok) wolf3d_SetOPLError(wg_failure);
    pthread_mutex_unlock(&wg_lock);
    if (!ok) WG_LinuxOPLShutdown();
    return ok;
}

void WG_LinuxOPLWriteTimed(uint16_t reg, uint8_t value, uint64_t frame, uint32_t rate)
{
    if (!wg_running) return;
    pthread_mutex_lock(&wg_lock);
    if (reg > 0xff || !WG_OPLTimelinePush(&wg_timeline, reg, value, frame, rate))
    {
        wg_failed = 1;
        wg_failure = "Linux OPL event queue overflow or invalid timestamp/rate";
    }
    pthread_mutex_unlock(&wg_lock);
}

void WG_LinuxOPLWrite(uint16_t reg, uint8_t value)
{
    uint64_t frame;
    if (!wg_running) return;
    pthread_mutex_lock(&wg_lock);
    frame = wg_timeline.count ? wg_timeline.events[(wg_timeline.head + wg_timeline.count - 1) % WG_OPL_QUEUE_CAPACITY].frame
                              : wg_timeline.submitted;
    pthread_mutex_unlock(&wg_lock);
    WG_LinuxOPLWriteTimed(reg, value, frame, wg_rate);
}

int WG_LinuxOPLSubmit(size_t frames, uint64_t queued)
{
    int ok;
    uint64_t now;
    if (!wg_running) return 1;
    pthread_mutex_lock(&wg_lock);
    now = WG_OPLNow();
    ok = !wg_failed && now && frames <= UINT_MAX;
    if (ok)
    {
        WG_OPLTimelineSubmit(&wg_timeline, (unsigned)frames, now);
        if (queued <= wg_timeline.submitted)
            WG_OPLTimelineObserve(&wg_timeline, (uint32_t)(wg_timeline.submitted - queued), now);
    }
    else wolf3d_SetOPLError(wg_failure ? wg_failure : "Linux OPL playback clock/queue failed");
    pthread_cond_broadcast(&wg_wake);
    pthread_mutex_unlock(&wg_lock);
    return ok;
}
