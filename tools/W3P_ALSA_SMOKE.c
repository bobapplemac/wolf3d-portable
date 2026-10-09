/* Packaging-only smoke test: no display or real sound device is opened. */
#include <alsa/asoundlib.h>
#include <stdio.h>
int main(void)
{
    snd_config_t *definition;
    snd_pcm_t *pcm;
    int result;
    if (snd_config_update() < 0 ||
        snd_config_search_definition(snd_config, "pcm", "plughw:0,0", &definition) < 0) {
        fputs("Bundled ALSA plughw definition is invalid\n", stderr);
        return 1;
    }
    snd_config_delete(definition);
    result = snd_pcm_open(&pcm, "null", SND_PCM_STREAM_PLAYBACK, 0);
    if (result < 0) {
        fprintf(stderr, "Bundled ALSA null PCM: %s\n", snd_strerror(result));
        return 1;
    }
    result = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED, 2, 48000, 1, 50000);
    snd_pcm_close(pcm);
    if (result < 0) {
        fprintf(stderr, "Bundled ALSA PCM parameters: %s\n", snd_strerror(result));
        return 1;
    }
    puts("Bundled ALSA configuration and null PCM passed");
    return 0;
}
