#include <windows.h>
#include <mmsystem.h>
static int wave_fail_open,wave_fail_prepare,wave_fail_write;
static unsigned wave_opens,wave_prepares,wave_unprepares,wave_writes,wave_closes;
static MMRESULT WINAPI mock_wave_open(LPHWAVEOUT out,UINT device,LPCWAVEFORMATEX format,DWORD_PTR callback,DWORD_PTR instance,DWORD flags)
{
    (void)device;(void)callback;(void)instance;(void)flags;
    ++wave_opens;
    assert(format->nChannels==2 && format->wBitsPerSample==16);
    if(wave_fail_open)return MMSYSERR_ERROR;
    *out=(HWAVEOUT)1;return MMSYSERR_NOERROR;
}
static MMRESULT WINAPI mock_wave_prepare(HWAVEOUT wave,LPWAVEHDR header,UINT size)
{
    (void)wave;(void)size;++wave_prepares;
    if(wave_fail_prepare && wave_prepares==(unsigned)wave_fail_prepare)return MMSYSERR_ERROR;
    header->dwFlags|=WHDR_PREPARED;return MMSYSERR_NOERROR;
}
static MMRESULT WINAPI mock_wave_unprepare(HWAVEOUT wave,LPWAVEHDR header,UINT size)
{ (void)wave;(void)size;++wave_unprepares;header->dwFlags&=~WHDR_PREPARED;return MMSYSERR_NOERROR; }
static MMRESULT WINAPI mock_wave_write(HWAVEOUT wave,LPWAVEHDR header,UINT size)
{ (void)wave;(void)header;(void)size;++wave_writes;return wave_fail_write?MMSYSERR_ERROR:MMSYSERR_NOERROR; }
static MMRESULT WINAPI mock_wave_reset(HWAVEOUT wave) {(void)wave;return MMSYSERR_NOERROR;}
static MMRESULT WINAPI mock_wave_close(HWAVEOUT wave) {(void)wave;++wave_closes;return MMSYSERR_NOERROR;}
#define waveOutOpen mock_wave_open
#define waveOutPrepareHeader mock_wave_prepare
#define waveOutUnprepareHeader mock_wave_unprepare
#define waveOutWrite mock_wave_write
#define waveOutReset mock_wave_reset
#define waveOutClose mock_wave_close
