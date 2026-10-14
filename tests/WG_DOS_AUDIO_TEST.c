#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "../platforms/dos/WG_DOS_SB16.c"
#include "../platforms/dos/WG_DOS_AUDIO_CLOCK.h"

static unsigned ports[65536], writes[65536], dsp[64], dsp_count;
static unsigned input[8], input_count, input_at, dsp_major = 3, dsp_minor = 1;
static unsigned mixer_index, mixer[256], reads[65536];
static int busy, allocated, interrupts_disabled, chained;
static unsigned dsp_parameters, fail_after;
static void old_irq(void) { ++chained; }
static void (*vectors[256])(void);
static void *dma_memory;

int inp(unsigned port)
{
    ++reads[port];
    if (port == 0x22c) return busy || (fail_after && dsp_count >= fail_after) ? 0x80 : 0;
    if (port == 0x22e) return input_at < input_count ? 0x80 : 0;
    if (port == 0x22a) { assert(input_at < input_count); return input[input_at++]; }
    if (port == 0x225) return mixer[mixer_index];
    return ports[port];
}
int outp(unsigned port, int value)
{
    unsigned previous = ports[port];
    ports[port] = value & 255;
    ++writes[port];
    if (port == 0x226 && previous == 1 && value == 0)
    {
        input_at = 0; input_count = 1; input[0] = 0xaa;
        dsp_parameters = 0;
    }
    if (port == 0x224) mixer_index = value;
    if (port == 0x225) mixer[mixer_index] = value;
    if (port == 0x22c)
    {
        assert(dsp_count < 64);
        dsp[dsp_count++] = value;
        if (dsp_parameters) --dsp_parameters;
        else if (value == 0xe1)
        {
            input_at = 0; input_count = 2;
            input[0] = dsp_major; input[1] = dsp_minor;
        }
        else if (value == 0x40) dsp_parameters = 1;
        else if (value == 0x41 || value == 0x48) dsp_parameters = 2;
        else if (value == 0xb6) dsp_parameters = 3;
    }
    return value;
}
void _disable(void) { interrupts_disabled = 1; }
void _enable(void) { interrupts_disabled = 0; }
void (*_dos_getvect(unsigned vector))(void) { return vectors[vector]; }
void _dos_setvect(unsigned vector, void (*handler)(void)) { vectors[vector] = handler; }
int int386(int number, union REGS *in, union REGS *out)
{
    unsigned operation = in->w.ax;
    assert(number == 0x31);
    if (operation == 0x100)
    {
        assert(!allocated);
        memset(out, 0, sizeof(*out));
        out->w.dx = 1;
        out->x.eax = 0x3000;
        allocated = 1;
    }
    else { assert(operation == 0x101); allocated = 0; }
    return 0;
}
static void environment(const char *value)
{
#ifdef _WIN32
    assert(_putenv_s("BLASTER", value) == 0);
#else
    assert(setenv("BLASTER", value, 1) == 0);
#endif
}
static void reset(void)
{
    unsigned i;
    WG_DOSSB16Shutdown();
    memset(ports, 0, sizeof(ports)); memset(writes, 0, sizeof(writes));
    memset(reads, 0, sizeof(reads)); memset(mixer, 0, sizeof(mixer));
    input_at = input_count = dsp_count = dsp_parameters = 0;
    busy = chained = 0; fail_after = 0;
    for (i = 0; i < 256; ++i) vectors[i] = old_irq;
    ports[0x21] = 0xff; ports[0xa1] = 0xff;
    dsp_major = 3; dsp_minor = 1;
}
int main(void)
{
    wolf3d_pcm_format_t request, obtained;
    int16_t samples[2048];
    uint32_t deadline, fraction;
    unsigned i;
#ifdef _WIN32
    dma_memory = VirtualAlloc((void *)0x30000, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    dma_memory = mmap((void *)0x30000, 8192, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
#endif
    assert(dma_memory == (void *)0x30000);
    request.sample_rate = 44100; request.channels = 2; request.bits_per_sample = 16;
    reset(); environment("A220 I5 D1 T4");
    mixer[0x0e] = 0x22;
    assert(WG_DOSSB16Init(&request, &obtained));
    assert(wg_sb16_8bit && wg_sb16_dma == 1 && obtained.sample_rate == 21739);
    assert(obtained.channels == 2 && obtained.bits_per_sample == 16);
    assert(mixer[0x0e] == 0x20);
    assert(dsp_count == 8 && dsp[2] == 0x40 && dsp[3] == 210);
    assert(dsp[4] == 0x48 && dsp[5] == 0xff && dsp[6] == 3 && dsp[7] == 0x1c);
    assert(ports[0x0b] == 0x59 && ports[0x83] == 3 && ports[0x03] == 7);
    assert(((uint8_t *)dma_memory)[0] == 128 && vectors[13] != old_irq);
    memset(samples, 0, sizeof(samples));
    samples[0] = samples[1] = -32768; samples[2] = samples[3] = 32767;
    assert(WG_DOSSB16Submit(samples, 1024));
    assert(((uint8_t *)dma_memory)[1024] == 0);
    assert(((uint8_t *)dma_memory)[1025] == 255);
    assert(((uint8_t *)dma_memory)[1026] == 128);
    assert(!WG_DOSSB16WritableFrames());
    vectors[13]();
    assert(WG_DOSSB16WritableFrames() == 1024 && !chained && reads[0x22e]);
    assert(!WG_DOSSB16Submit(samples, 512));
    WG_DOSSB16Shutdown();
    assert(!allocated && vectors[13] == old_irq && ports[0x21] == 255);
    assert(mixer[0x0e] == 0x22 && ports[0x0a] == 5 && !interrupts_disabled);

    reset(); dsp_major = 4; environment("A220 I5 D1 H5 T6");
    assert(WG_DOSSB16Init(&request, &obtained));
    assert(!wg_sb16_8bit && obtained.sample_rate == 44100 && wg_sb16_dma == 5);
    assert(dsp[2] == 0x41 && dsp[3] == 0xac && dsp[4] == 0x44 && dsp[5] == 0xb6);
    assert(ports[0xd6] == 0x59 && ports[0xc6] == 15);
    assert(WG_DOSSB16Submit(samples, 1024));
    assert(memcmp((uint8_t *)dma_memory + 4096, samples, sizeof(samples)) == 0);
    mixer[0x82] = 0; vectors[13](); assert(chained == 1);
    mixer[0x82] = 2; vectors[13](); assert(reads[0x22f] == 1);
    WG_DOSSB16Shutdown(); assert(!allocated && vectors[13] == old_irq);

    reset(); dsp_major = 4; environment("A220 I5 D1 T6");
    assert(WG_DOSSB16Init(&request, &obtained) && wg_sb16_8bit);
    reset(); dsp_major = 2; environment("A220 I5 D1 T3");
    assert(WG_DOSSB16Init(&request, &obtained) && wg_sb16_8bit);
    reset(); environment("A220 I5 D1 T4"); fail_after = 2;
    assert(!WG_DOSSB16Init(&request, &obtained));
    assert(!allocated && vectors[13] == old_irq && ports[0x21] == 255);
    assert(strstr(WG_DOSSB16Status(), "timed out") != NULL);
    reset(); dsp_major = 1; assert(!WG_DOSSB16Init(&request, &obtained));
    reset(); environment("A220 I5 H5 T4");
    assert(!WG_DOSSB16Init(&request, &obtained) && !allocated);
    environment("A220 I5 D257 T4"); assert(!WG_SB16ParseBLASTER());
    environment("A220 I5 D2 T4"); assert(!WG_SB16ParseBLASTER());
    environment("A220 I261 D1 T4"); assert(!WG_SB16ParseBLASTER());
    environment("a220 i2 d0 t4"); assert(WG_SB16ParseBLASTER() && wg_sb16_irq == 9);
    busy = 1; dsp_count = 0; assert(!WG_SB16Write(0xd1) && !dsp_count);
    input_count = input_at = 0;
    { uint8_t value; assert(!WG_SB16Read(&value)); }

    /* One minute at 44100 Hz, independent of simulated submission lateness. */
    deadline = 100; fraction = 0;
    for (i = 0; i < 2583; ++i) WG_DOSAudioClockAdvance(&deadline, &fraction, 44100, 1024);
    assert(deadline == 100 + (2583UL * 1024UL * 1000UL) / 44100UL);
    assert(fraction == (2583UL * 1024UL * 1000UL) % 44100UL);
    deadline = UINT32_MAX - 10; fraction = 0;
    WG_DOSAudioClockAdvance(&deadline, &fraction, 1000, 20);
    assert(deadline == 9);
#ifdef _WIN32
    VirtualFree(dma_memory, 0, MEM_RELEASE);
#else
    munmap(dma_memory, 8192);
#endif
    puts("DOS audio: SB2/Pro, SB16, DMA, IRQ, conversion, failures and clock tests passed.");
    return 0;
}
