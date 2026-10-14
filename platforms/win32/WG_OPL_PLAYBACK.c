/* PCM is rendered ahead; only this worker writes queued musical events to
   the chip. Probe/reset/shutdown remain synchronous with the worker stopped. */
#include "WG_OPL_PLAYBACK.h"
#include <stdio.h>
#include <stdlib.h>
#include "../WG_OPL_TIMELINE.h"
#include "WG_INPOUT32.h"
#include "../dos/WG_DOS_ADLIB.h"
static wg_opl_timeline_t wg_timeline;
static CRITICAL_SECTION wg_opl_lock;
static HANDLE wg_opl_thread, wg_opl_wake;
static HWAVEOUT wg_opl_wave;
static int wg_opl_stopping, wg_opl_failed, wg_opl_period;
/* Opt-in, in-memory counters only during playback. Write once after joining. */
static int wg_trace;
static uint64_t wg_trace_start, wg_trace_writes, wg_trace_queries, wg_trace_positions;
static uint64_t wg_trace_write_ticks, wg_trace_query_ticks, wg_trace_max_write;
static uint64_t wg_trace_late_frames, wg_trace_peak_queue, wg_trace_starvations;
static void WG_OPLPlaybackReport(void)
{
    FILE *file;
    LARGE_INTEGER now;
    double elapsed;
    if (!wg_trace || !wg_trace_start || !QueryPerformanceCounter(&now)) return;
    file = fopen("OPLTIME.TXT", "a");
    if (!file) return;
    elapsed = (double)((uint64_t)now.QuadPart-wg_trace_start)/wg_timeline.frequency;
    fprintf(file, "Native OPL timing: rate=%lu, QPC=%.0f Hz, elapsed=%.6f s\n",
        (unsigned long)wg_timeline.rate, (double)wg_timeline.frequency, elapsed);
    fprintf(file, "PCM submitted=%.6f s; last device position=%.6f s; last emitted timeline=%.6f s\n",
        (double)wg_timeline.submitted/wg_timeline.rate,
        (double)wg_timeline.observed/wg_timeline.rate,
        (double)wg_timeline.emitted/wg_timeline.rate);
    fprintf(file, "Writes=%.0f; write work=%.6f s; longest write=%.3f ms; maximum event lateness=%.3f ms\n",
        (double)wg_trace_writes, (double)wg_trace_write_ticks/wg_timeline.frequency,
        (double)wg_trace_max_write*1000/wg_timeline.frequency,
        (double)wg_trace_late_frames*1000/wg_timeline.rate);
    fprintf(file, "Position queries=%.0f (sample reports=%.0f); query work=%.6f s; peak queued writes=%.0f; PCM starvation/restarts=%.0f; failed=%d\n\n",
        (double)wg_trace_queries, (double)wg_trace_positions, (double)wg_trace_query_ticks/wg_timeline.frequency,
        (double)wg_trace_peak_queue, (double)wg_trace_starvations, wg_opl_failed);
    fclose(file);
}

static DWORD WINAPI WG_OPLPlaybackThread(LPVOID context)
{
    uint64_t last_query = 0, last_counter = 0;
    (void)context;
    for (;;)
    {
        LARGE_INTEGER now;
        MMTIME position;
        wg_opl_event_t event;
        int ready, started;
        EnterCriticalSection(&wg_opl_lock);
        if (wg_opl_stopping || wg_opl_failed)
        {
            LeaveCriticalSection(&wg_opl_lock); return 0;
        }
        if (!QueryPerformanceCounter(&now) || (uint64_t)now.QuadPart < last_counter)
        {
            wg_opl_failed = 1;
            LeaveCriticalSection(&wg_opl_lock); return 0;
        }
        last_counter = (uint64_t)now.QuadPart;
        started = wg_timeline.started;
        if (wg_opl_wave && started && (uint64_t)now.QuadPart - last_query >= wg_timeline.frequency / 1000)
        {
            last_query = (uint64_t)now.QuadPart;
            memset(&position, 0, sizeof(position)); position.wType = TIME_SAMPLES;
            if (waveOutGetPosition(wg_opl_wave, &position, sizeof(position)) == MMSYSERR_NOERROR
                && position.wType == TIME_SAMPLES) {
                WG_OPLTimelineObserve(&wg_timeline, position.u.sample, (uint64_t)now.QuadPart);
                if (wg_trace) ++wg_trace_positions;
            }
            if (wg_trace) {
                LARGE_INTEGER end;
                ++wg_trace_queries;
                if (QueryPerformanceCounter(&end) && end.QuadPart >= now.QuadPart)
                    wg_trace_query_ticks += (uint64_t)(end.QuadPart-now.QuadPart);
            }
        }
        ready = WG_OPLTimelinePop(&wg_timeline, (uint64_t)now.QuadPart, &event);
        if (wg_trace && ready) {
            uint64_t late = wg_timeline.emitted > event.frame ? wg_timeline.emitted-event.frame : 0;
            if (late > wg_trace_late_frames) wg_trace_late_frames = late;
        }
        LeaveCriticalSection(&wg_opl_lock);
        if (ready) {
            LARGE_INTEGER begin, end;
            int measured = wg_trace && QueryPerformanceCounter(&begin);
            WG_DOSAdLibWrite(event.reg, event.value);
            if (wg_trace) {
                ++wg_trace_writes;
                if (measured && QueryPerformanceCounter(&end) && end.QuadPart >= begin.QuadPart) {
                    uint64_t ticks = (uint64_t)(end.QuadPart-begin.QuadPart);
                    wg_trace_write_ticks += ticks;
                    if (ticks > wg_trace_max_write) wg_trace_max_write = ticks;
                }
            }
        }
        else
            WaitForSingleObject(wg_opl_wake, started ? 1 : INFINITE);
    }
}
void WG_OPLPlaybackStop(void)
{
    if (!wg_opl_thread) return;
    EnterCriticalSection(&wg_opl_lock); wg_opl_stopping = 1;
    LeaveCriticalSection(&wg_opl_lock);
    SetEvent(wg_opl_wake);
    WaitForSingleObject(wg_opl_thread, INFINITE);
    WG_OPLPlaybackReport();
    CloseHandle(wg_opl_thread); CloseHandle(wg_opl_wake);
    wg_opl_thread = wg_opl_wake = NULL;
    DeleteCriticalSection(&wg_opl_lock);
    if (wg_opl_period) timeEndPeriod(1);
    wg_opl_period = 0;
    memset(&wg_timeline, 0, sizeof(wg_timeline));
}
int WG_OPLPlaybackStart(HWAVEOUT wave, uint32_t rate)
{
    LARGE_INTEGER frequency;
    WG_OPLPlaybackStop();
    if (!rate || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        return 0;
    WG_OPLTimelineInit(&wg_timeline, rate, (uint64_t)frequency.QuadPart);
    wg_opl_wave = wave; wg_opl_stopping = wg_opl_failed = 0;
    { const char *trace = getenv("WOLF3D_OPL_TIMING"); wg_trace = trace && strcmp(trace, "1") == 0; }
    wg_trace_start = wg_trace_writes = wg_trace_queries = wg_trace_positions = 0;
    wg_trace_write_ticks = wg_trace_query_ticks = wg_trace_max_write = 0;
    wg_trace_late_frames = wg_trace_peak_queue = wg_trace_starvations = 0;
    InitializeCriticalSection(&wg_opl_lock);
    wg_opl_wake = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!wg_opl_wake) { DeleteCriticalSection(&wg_opl_lock); return 0; }
    wg_opl_period = timeBeginPeriod(1) == TIMERR_NOERROR;
    wg_opl_thread = CreateThread(NULL, 0, WG_OPLPlaybackThread, NULL, 0, NULL);
    if (!wg_opl_thread)
    {
        if (wg_opl_period) timeEndPeriod(1);
        wg_opl_period = 0; CloseHandle(wg_opl_wake); wg_opl_wake = NULL;
        DeleteCriticalSection(&wg_opl_lock); return 0;
    }
    (void)SetThreadPriority(wg_opl_thread, THREAD_PRIORITY_ABOVE_NORMAL);
    return 1;
}
void WG_OPLPlaybackWrite(uint16_t reg, uint8_t value, uint64_t frame, uint32_t rate)
{
    if (!wg_opl_thread) return;
    EnterCriticalSection(&wg_opl_lock);
    if (!WG_OPLTimelinePush(&wg_timeline, reg, value, frame, rate)) wg_opl_failed = 1;
    if (wg_trace && wg_timeline.count > wg_trace_peak_queue) wg_trace_peak_queue = wg_timeline.count;
    LeaveCriticalSection(&wg_opl_lock);
}
int WG_OPLPlaybackSubmit(const int16_t *samples, size_t frames, WAVEHDR *header)
{
    LARGE_INTEGER now;
    int ok;
    (void)samples;
    EnterCriticalSection(&wg_opl_lock);
    ok = !wg_opl_failed && QueryPerformanceCounter(&now);
    if (ok)
    {
        /* Hold publication until the matching PCM has actually been accepted. */
        ok = waveOutWrite(wg_opl_wave, header, sizeof(*header)) == MMSYSERR_NOERROR;
        if (ok) {
            if (wg_trace) {
                if (!wg_timeline.started) wg_trace_start = (uint64_t)now.QuadPart;
                else if (WG_OPLTimelineNow(&wg_timeline, (uint64_t)now.QuadPart) >= wg_timeline.submitted)
                    ++wg_trace_starvations;
            }
            WG_OPLTimelineSubmit(&wg_timeline, (unsigned)frames, (uint64_t)now.QuadPart);
        }
    }
    LeaveCriticalSection(&wg_opl_lock);
    if (!ok) WG_InpOutError("native OPL playback queue/clock failed");
    SetEvent(wg_opl_wake);
    return ok;
}

/* SDL accepts PCM itself, then publishes its application-rate queue position.
   Unlike waveOut, no SDL API is called from the worker thread. */
int WG_OPLPlaybackSubmitFrames(size_t frames, uint64_t queued)
{
    LARGE_INTEGER now;
    int ok;
    if (!wg_opl_thread) return 1;
    EnterCriticalSection(&wg_opl_lock);
    ok = !wg_opl_failed && frames <= 0xffffffffU && QueryPerformanceCounter(&now);
    if (ok)
    {
        WG_OPLTimelineSubmit(&wg_timeline, (unsigned)frames, (uint64_t)now.QuadPart);
        if (queued <= wg_timeline.submitted)
            WG_OPLTimelineObserve(&wg_timeline, (uint32_t)(wg_timeline.submitted - queued), (uint64_t)now.QuadPart);
    }
    LeaveCriticalSection(&wg_opl_lock);
    if (!ok) WG_InpOutError("native OPL playback queue/clock failed");
    SetEvent(wg_opl_wake);
    return ok;
}
