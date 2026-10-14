#ifndef WG_DOS_AUDIO_CLOCK_H
#define WG_DOS_AUDIO_CLOCK_H

/* Advance the previous deadline, not the submission time. Carry fractional
   milliseconds so neither block rounding nor rendering time changes tempo.
   Callers submit at most 1024 frames, keeping the arithmetic within 32 bits. */
static void WG_DOSAudioClockAdvance(uint32_t *deadline, uint32_t *fraction,
                                    uint32_t rate, uint32_t frames)
{
    uint32_t elapsed = *fraction + frames * 1000U;
    *deadline += elapsed / rate;
    *fraction = elapsed % rate;
}

#endif
