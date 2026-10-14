/* No real port I/O or elevated privileges: exercise the actual pthread worker. */
#define _POSIX_C_SOURCE 200809L
#define WG_LINUX_OPL_TEST 1
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef WG_LINUX_OPL_LEGACY
#include <sys/time.h>
#endif
static int permitted = 1, present = 1, opl3 = 1;
static int grants, releases, reads, writes, bank2, markers;
static int clock_fault_test;
static unsigned char address;
static pthread_t owner;
static uint64_t last_port_ns, marker_ns;
static unsigned last_port;
static char error[256];
static pthread_mutex_t trace_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t test_now(void)
{
#ifdef WG_LINUX_OPL_LEGACY
    struct timeval tv;
    assert(gettimeofday(&tv, NULL) == 0);
    return (uint64_t)tv.tv_sec * 1000000000U + (uint64_t)tv.tv_usec * 1000U;
#else
    struct timespec t;
    assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return (uint64_t)t.tv_sec * 1000000000U + (uint64_t)t.tv_nsec;
#endif
}
static int ioperm(unsigned long port, unsigned long count, int on)
{
    assert(port == 0x388 && count == 4);
    if (on)
    {
        if (!permitted) { errno = EPERM; return -1; }
        owner = pthread_self(); ++grants;
    }
    else { assert(pthread_equal(owner, pthread_self())); ++releases; }
    return 0;
}
static unsigned char inb(unsigned short port)
{
    assert(port == 0x388 && pthread_equal(owner, pthread_self()));
    return present ? (unsigned char)((reads++ % 2 ? 0xc0 : 0) | (opl3 ? 0 : 6)) : 0xff;
}
static void outb(unsigned char value, unsigned short port)
{
    uint64_t now = test_now();
    assert(pthread_equal(owner, pthread_self()));
    assert(port >= 0x388 && port <= 0x38b);
    if (last_port_ns && !clock_fault_test)
        assert(now - last_port_ns >= ((last_port & 1) ? 24000U : 4000U));
    last_port_ns = now; last_port = port;
    ++writes;
    if (port >= 0x38a) ++bank2;
    if (port == 0x388) address = value;
    if (port == 0x389 && address == 0x20 && value == 0x7e)
    {
        pthread_mutex_lock(&trace_lock);
        ++markers; marker_ns = now;
        pthread_mutex_unlock(&trace_lock);
    }
}
void wolf3d_SetOPLError(const char *message)
{
    snprintf(error, sizeof(error), "%s", message);
}
#include "../platforms/linux/WG_LINUX_OPL.c"
static void nap(unsigned ms)
{
    struct timespec t = {0, (long)ms * 1000000L};
    nanosleep(&t, NULL);
}
static int marker_count(void)
{
    int count;
    pthread_mutex_lock(&trace_lock); count = markers; pthread_mutex_unlock(&trace_lock);
    return count;
}
int main(void)
{
    uint64_t start;
    unsigned retry;
#ifdef WG_LINUX_OPL_LEGACY
    assert(WG_OPLLegacyClock(10000000000ULL));
    assert(!WG_OPLLegacyClock(9999999999ULL)); /* Backwards clock fails closed. */
    assert(!WG_OPLLegacyClock(11000000000ULL)); /* Failure remains sticky. */
    wg_last_clock=0; wg_legacy_clock_failed=0;
    assert(WG_OPLLegacyClock(10000000000ULL));
    assert(!WG_OPLLegacyClock(16000000000ULL)); /* Large forward jump. */
    wg_last_clock=0; wg_legacy_clock_failed=0;
    assert(!WG_OPLLegacyClock(0)); /* System clock query failure. */
#endif
    assert(!WG_LinuxOPLInit());
    assert(strstr(error, "requires PCM") && grants == 0);
    WG_LinuxOPLSetRate(22050);
    permitted = 0;
    assert(!WG_LinuxOPLInit());
    assert(strstr(error, "CAP_SYS_RAWIO") && writes == 0 && grants == 0);
    permitted = 1; present = 0;
    assert(!WG_LinuxOPLInit());
    assert(strstr(error, "did not respond") && grants == releases);
    present = 1;
    assert(WG_LinuxOPLInit());
    assert(WG_LinuxOPLActive());
    assert(bank2 > 0); /* OPL3 mode reset is necessary for OPL2 routing. */
    WG_LinuxOPLWriteTimed(0x20, 0x7e, 2205, 22050);
    nap(20);
    assert(marker_count() == 0); /* Rendering alone cannot publish writes. */
    start = test_now();
    assert(WG_LinuxOPLSubmit(4410, 4410));
    for (retry = 0; retry < 100 && !marker_count(); ++retry) nap(5);
    assert(marker_count() == 1 && marker_ns - start >= 95000000U);
    /* Invalid rate fails instead of silently dropping events. */
    WG_LinuxOPLWriteTimed(0x20, 0x7e, 4410, 48000);
    assert(!WG_LinuxOPLSubmit(10, 10));
    assert(strstr(error, "timestamp/rate"));
    WG_LinuxOPLShutdown();
    assert(!WG_LinuxOPLActive() && grants == releases);
    /* OPL2 never receives bank-two writes; restart discards pending events. */
    bank2 = 0; opl3 = 0; reads = 0;
    assert(WG_LinuxOPLInit());
    assert(bank2 == 0);
    WG_LinuxOPLWriteTimed(0x20, 0x7e, 220500, 22050);
    WG_LinuxOPLShutdown();
    assert(marker_count() == 1 && grants == releases);
    WG_LinuxOPLShutdown();
#ifdef WG_LINUX_OPL_LEGACY
    assert(WG_LinuxOPLInit());
    { int previous=writes;
    /* Inject the sticky clock failure; cleanup must still silence/revoke. */
    pthread_mutex_lock(&wg_clock_lock);
    clock_fault_test=1;
    wg_legacy_clock_failed=1;
    pthread_mutex_unlock(&wg_clock_lock);
    assert(!WG_LinuxOPLSubmit(1,1));
      WG_LinuxOPLShutdown();
      assert(writes>previous && grants==releases); }
#endif
    puts("Linux OPL permission, detection, timing, PCM publication and cleanup tests passed");
    return 0;
}
