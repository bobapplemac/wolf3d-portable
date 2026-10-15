#ifndef WG_WAVEOUT_H
#define WG_WAVEOUT_H
/* Shared GDI/SDL Windows PCM transport. The host owns OPL worker lifetime. */
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define WG_PCM_BUFFER_COUNT 4U
#define WG_PCM_BUFFER_FRAMES 512U
typedef struct wg_waveout {
    HWAVEOUT handle;
    WAVEHDR headers[WG_PCM_BUFFER_COUNT];
    int16_t samples[WG_PCM_BUFFER_COUNT][WG_PCM_BUFFER_FRAMES*2U];
    uint8_t used[WG_PCM_BUFFER_COUNT];
} wg_waveout_t;
typedef int (*wg_waveout_submit_t)(const int16_t *,size_t,WAVEHDR *);
static void WG_WaveOutClose(wg_waveout_t *w);
static int WG_WaveOutOpen(wg_waveout_t *w, uint32_t sample_rate, uint16_t channels)
{
    WAVEFORMATEX format;
    size_t index;

    if (sample_rate == 0U || channels != 2U)
    {
        return 0;
    }
    WG_WaveOutClose(w);
    ZeroMemory(&format, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = sample_rate;
    format.wBitsPerSample = 16U;
    format.nBlockAlign = (WORD)(channels * sizeof(int16_t));
    format.nAvgBytesPerSec = sample_rate * format.nBlockAlign;
    if (waveOutOpen(&w->handle, WAVE_MAPPER, &format, 0, 0,
                    CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        w->handle = NULL;
        return 0;
    }
    ZeroMemory(w->headers, sizeof(w->headers));
    ZeroMemory(w->used, sizeof(w->used));
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        w->headers[index].lpData = (LPSTR)w->samples[index];
        w->headers[index].dwBufferLength =
            (DWORD)sizeof(w->samples[index]);
        if (waveOutPrepareHeader(w->handle, &w->headers[index],
                                 sizeof(w->headers[index]))
            != MMSYSERR_NOERROR)
        {
            WG_WaveOutClose(w);
            return 0;
        }
    }
    return 1;
}


static void WG_WaveOutClose(wg_waveout_t *w)
{
    size_t index;

    if (w->handle == NULL)
    {
        return;
    }
    waveOutReset(w->handle);
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        if ((w->headers[index].dwFlags & WHDR_PREPARED) != 0U)
        {
            waveOutUnprepareHeader(w->handle, &w->headers[index],
                                   sizeof(w->headers[index]));
        }
    }
    waveOutClose(w->handle);
    w->handle = NULL;
    ZeroMemory(w->headers, sizeof(w->headers));
    ZeroMemory(w->used, sizeof(w->used));
}


static size_t WG_WaveOutWritable(wg_waveout_t *w)
{
    size_t index;

    if (w->handle == NULL)
    {
        return 0U;
    }
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        if (w->used[index]
            && (w->headers[index].dwFlags & WHDR_DONE) != 0U)
        {
            w->used[index] = 0U;
        }
        if (!w->used[index])
        {
            return WG_PCM_BUFFER_FRAMES;
        }
    }
    return 0U;
}


static int WG_WaveOutSubmit(wg_waveout_t *w, const int16_t *samples, size_t frame_count, wg_waveout_submit_t publish)
{
    size_t index;

    if (w->handle == NULL || samples == NULL || frame_count == 0U
        || frame_count > WG_PCM_BUFFER_FRAMES)
    {
        return 0;
    }
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        if (!w->used[index])
        {
            size_t byte_count = frame_count * 2U * sizeof(*samples);

            memcpy(w->samples[index], samples, byte_count);
            w->headers[index].dwBufferLength = (DWORD)byte_count;
            w->headers[index].dwFlags &= ~WHDR_DONE;
            if (publish) {
                if (!publish(samples,frame_count,&w->headers[index])) return 0;
            } else
            if (waveOutWrite(w->handle, &w->headers[index],
                             sizeof(w->headers[index]))
                != MMSYSERR_NOERROR)
            {
                return 0;
            }
            w->used[index] = 1U;
            return 1;
        }
    }
    return 0;
}

#endif
