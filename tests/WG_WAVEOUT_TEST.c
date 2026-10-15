#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "WG_WAVEOUT_MOCK.h"
#include "../platforms/win32/WG_WAVEOUT.h"
static unsigned publications;
static int publish(const int16_t *samples,size_t frames,WAVEHDR *header)
{
    assert(frames==512 && header->dwBufferLength==2048);
    assert(!memcmp(samples,header->lpData,header->dwBufferLength));
    ++publications;return !wave_fail_write;
}
int main(void)
{
    wg_waveout_t w={0};int16_t data[1024];unsigned i;
    for(i=0;i<1024;++i)data[i]=(int16_t)(i+1);
    assert(!WG_WaveOutOpen(&w,0,2) && !WG_WaveOutOpen(&w,22050,1));
    wave_fail_open=1;assert(!WG_WaveOutOpen(&w,22050,2) && !w.handle);wave_fail_open=0;
    wave_fail_prepare=3;assert(!WG_WaveOutOpen(&w,22050,2));
    assert(!w.handle && wave_unprepares==2 && wave_closes==1);wave_fail_prepare=0;
    assert(WG_WaveOutOpen(&w,22050,2));
    for(i=0;i<4;++i) {
        assert(WG_WaveOutWritable(&w)==512);
        assert(WG_WaveOutSubmit(&w,data,512,NULL));
        assert(!memcmp(w.samples[i],data,sizeof(data)));
    }
    assert(!WG_WaveOutWritable(&w) && !WG_WaveOutSubmit(&w,data,1,NULL));
    w.headers[0].dwFlags|=WHDR_DONE;assert(WG_WaveOutWritable(&w)==512);
    wave_fail_write=1;assert(!WG_WaveOutSubmit(&w,data,512,publish) && !w.used[0]);
    wave_fail_write=0;assert(WG_WaveOutSubmit(&w,data,512,publish));assert(publications==2);
    assert(wave_writes==4); /* OPL publication owns waveOutWrite; never write twice. */
    WG_WaveOutClose(&w);assert(!w.handle && !WG_WaveOutWritable(&w));
    WG_WaveOutClose(&w);assert(wave_closes==2);
    assert(WG_WaveOutOpen(&w,44100,2));WG_WaveOutClose(&w);
    puts("Shared waveOut: bounded queue, samples, completion, publication, failure cleanup and restart passed.");
    return 0;
}
