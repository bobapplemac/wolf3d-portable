/* Real Windows worker, fake waveOut device and register sink: no InpOut DLL. */
#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
static LARGE_INTEGER frequency, epoch;
static volatile LONG accepted, count;
static LONGLONG times[8];
static MMRESULT WINAPI fake_position(HWAVEOUT device, LPMMTIME pos, UINT size)
{
    LARGE_INTEGER now;
    LONGLONG frame;
    (void)device; (void)size;
    QueryPerformanceCounter(&now);
    frame = (now.QuadPart - epoch.QuadPart) * 11025 / frequency.QuadPart;
    if (frame > accepted) frame = accepted;
    pos->wType = TIME_SAMPLES; pos->u.sample = (DWORD)frame;
    return MMSYSERR_NOERROR;
}
static MMRESULT WINAPI fake_submit(HWAVEOUT device, LPWAVEHDR header, UINT size)
{
    (void)device; (void)size;
    if (!accepted) QueryPerformanceCounter(&epoch);
    InterlockedExchangeAdd(&accepted, (LONG)(header->dwBufferLength / 4));
    return MMSYSERR_NOERROR;
}
void WG_DOSAdLibWrite(uint16_t reg, uint8_t value)
{
    LARGE_INTEGER now;
    (void)reg; (void)value;
    QueryPerformanceCounter(&now);
    assert(count < 8); times[count] = now.QuadPart;
    InterlockedIncrement(&count);
}
void WG_InpOutError(const char *message) { (void)message; assert(0); }
#define waveOutGetPosition fake_position
#define waveOutWrite fake_submit
#include "../platforms/win32/WG_OPL_PLAYBACK.c"
int main(void)
{
    WAVEHDR header;
    LONG before;
    DWORD start;
    QueryPerformanceFrequency(&frequency);
    assert(WG_OPLPlaybackStart((HWAVEOUT)1, 11025));
    WG_OPLPlaybackWrite(1, 1, 0, 11025);
    WG_OPLPlaybackWrite(1, 2, 220, 11025);
    WG_OPLPlaybackWrite(1, 3, 440, 11025);
    Sleep(15); assert(!count); /* No PCM submission: no early notes. */
    memset(&header, 0, sizeof(header)); header.dwBufferLength = 512 * 4;
    assert(WG_OPLPlaybackSubmit(NULL, 512, &header));
    start = GetTickCount();
    while (count < 3 && GetTickCount() - start < 2000) Sleep(1);
    assert(count == 3);
    assert((times[1] - times[0]) * 1000 / frequency.QuadPart >= 5);
    assert((times[2] - times[1]) * 1000 / frequency.QuadPart >= 5);
    WG_OPLPlaybackWrite(1, 4, 10000, 11025);
    WG_OPLPlaybackStop(); before = count; Sleep(20); assert(count == before);
    accepted = 0;
    assert(WG_OPLPlaybackStart((HWAVEOUT)1, 11025));
    assert(WG_OPLPlaybackSubmit(NULL, 512, &header));
    Sleep(20); WG_OPLPlaybackStop(); assert(count == before);
    puts("OPL worker: events paced during PCM playback; stop/restart cancel pending writes.");
    return 0;
}
