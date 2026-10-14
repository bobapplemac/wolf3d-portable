/* Build with Open Watcom alongside platforms/dos/WG_DOS_SB16.c.
   Runs real DMA/IRQ playback for 32 blocks, then verifies bounded teardown.
   Use only with the machine's actual BLASTER configuration. */
#include "WG_DOS_SB16.h"
#include <stdio.h>
#include <time.h>

int main(void)
{
    wolf3d_pcm_format_t requested;
    wolf3d_pcm_format_t obtained;
    int16_t samples[2048];
    unsigned blocks = 0U;
    unsigned index;
    clock_t start;
    requested.sample_rate = 44100U;
    requested.channels = 2U;
    requested.bits_per_sample = 16U;
    if (!WG_DOSSB16Init(&requested, &obtained))
    {
        printf("FAIL: %s\n", WG_DOSSB16Status());
        return 1;
    }
    printf("%s\n", WG_DOSSB16Status());
    /* Quiet square wave, alternating channels, exercises stereo downmix. */
    for (index = 0U; index < 1024U; ++index)
    {
        samples[index * 2U] = (index & 32U) ? 2048 : -2048;
        samples[index * 2U + 1U] = 0;
    }
    start = clock();
    while (blocks < 32U && clock() - start < 10L * CLOCKS_PER_SEC)
    {
        if (WG_DOSSB16WritableFrames() != 0U)
        {
            if (!WG_DOSSB16Submit(samples, 1024U)) break;
            ++blocks;
        }
    }
    WG_DOSSB16Shutdown();
    printf("%s: %u DMA blocks submitted\n", blocks == 32U ? "PASS" : "FAIL", blocks);
    return blocks == 32U ? 0 : 1;
}
