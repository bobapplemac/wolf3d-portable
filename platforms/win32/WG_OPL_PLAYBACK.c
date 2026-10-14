/* PCM is rendered ahead; only this worker writes queued musical events to
   the chip. Probe/reset/shutdown remain synchronous with the worker stopped. */
#include "WG_OPL_PLAYBACK.h"
#include "WG_OPL_TIMELINE.h"
#include "WG_INPOUT32.h"
#include "../dos/WG_DOS_ADLIB.h"
static wg_opl_timeline_t wg_timeline;
static CRITICAL_SECTION wg_opl_lock;
static HANDLE wg_opl_thread, wg_opl_wake;
static HWAVEOUT wg_opl_wave;
static int wg_opl_stopping, wg_opl_failed, wg_opl_period;

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
        if (started && (uint64_t)now.QuadPart - last_query >= wg_timeline.frequency / 1000)
        {
            last_query = (uint64_t)now.QuadPart;
            memset(&position, 0, sizeof(position)); position.wType = TIME_SAMPLES;
            if (waveOutGetPosition(wg_opl_wave, &position, sizeof(position)) == MMSYSERR_NOERROR
                && position.wType == TIME_SAMPLES)
                WG_OPLTimelineObserve(&wg_timeline, position.u.sample, (uint64_t)now.QuadPart);
        }
        ready = WG_OPLTimelinePop(&wg_timeline, (uint64_t)now.QuadPart, &event);
        LeaveCriticalSection(&wg_opl_lock);
        if (ready)
            WG_DOSAdLibWrite(event.reg, event.value);
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
    if (!wave || !rate || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        return 0;
    WG_OPLTimelineInit(&wg_timeline, rate, (uint64_t)frequency.QuadPart);
    wg_opl_wave = wave; wg_opl_stopping = wg_opl_failed = 0;
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
        if (ok) WG_OPLTimelineSubmit(&wg_timeline, (unsigned)frames, (uint64_t)now.QuadPart);
    }
    LeaveCriticalSection(&wg_opl_lock);
    if (!ok) WG_InpOutError("native OPL playback queue/clock failed");
    SetEvent(wg_opl_wake);
    return ok;
}
