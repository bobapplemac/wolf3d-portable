#include "WG_DOS_SB16.h"

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WG_SB16_CHANNELS 2U
#define WG_SB16_BITS 16U
#define WG_SB16_HALF_FRAMES 1024U
#define WG_SB16_HALF_BYTES \
    (WG_SB16_HALF_FRAMES * WG_SB16_CHANNELS * (WG_SB16_BITS / 8U))
#define WG_SB16_DMA_BYTES (WG_SB16_HALF_BYTES * 2U)
#define WG_SB16_DMA_BOUNDARY 0x10000UL
#define WG_SB16_MAX_DOS_BLOCKS 16U
#define WG_SB16_IO_TIMEOUT 65535U

typedef void (__interrupt __far *wg_sb16_interrupt_t)(void);

static const uint16_t wg_sb16_dma_address_ports[3] =
{
    0x00c4U, 0x00c8U, 0x00ccU
};
static const uint16_t wg_sb16_dma_count_ports[3] =
{
    0x00c6U, 0x00caU, 0x00ceU
};
static const uint16_t wg_sb16_dma_page_ports[3] =
{
    0x008bU, 0x0089U, 0x008aU
};

static uint16_t wg_sb16_base;
static uint8_t wg_sb16_irq;
static uint8_t wg_sb16_dma;
static uint8_t wg_sb16_low_dma;
static uint8_t wg_sb16_high_dma;
static uint8_t wg_sb16_8bit;
static uint8_t wg_sb16_dsp_major;
static uint8_t wg_sb16_mixer_saved;
static uint8_t wg_sb16_mixer_output;
static unsigned wg_sb16_half_bytes;
static char wg_sb16_status[160] = "not initialized";

const char *WG_DOSSB16Status(void)
{
    return wg_sb16_status;
}
static uint8_t wg_sb16_vector;
static uint8_t wg_sb16_master_mask;
static uint8_t wg_sb16_slave_mask;
static wg_sb16_interrupt_t wg_sb16_old_interrupt;
static uint8_t *wg_sb16_buffer;
static uint32_t wg_sb16_buffer_physical;
static uint16_t wg_sb16_dos_selectors[WG_SB16_MAX_DOS_BLOCKS];
static uint8_t wg_sb16_dos_block_count;
static volatile uint8_t wg_sb16_play_half;
static volatile uint8_t wg_sb16_half_free[2];
static uint8_t wg_sb16_active;

static int WG_SB16Write(uint8_t value)
{
    unsigned wait;
    for (wait = 0U; wait < WG_SB16_IO_TIMEOUT; ++wait)
    {
        if ((inp(wg_sb16_base + 0x0cU) & 0x80U) == 0U)
        {
            outp(wg_sb16_base + 0x0cU, value);
            return 1;
        }
    }
    return 0;
}

static int WG_SB16Read(uint8_t *value)
{
    unsigned wait;
    for (wait = 0U; wait < WG_SB16_IO_TIMEOUT; ++wait)
    {
        if ((inp(wg_sb16_base + 0x0eU) & 0x80U) != 0U)
        {
            *value = (uint8_t)inp(wg_sb16_base + 0x0aU);
            return 1;
        }
    }
    return 0;
}

static int WG_SB16ResetDSP(void)
{
    unsigned wait;
    uint8_t value;

    outp(wg_sb16_base + 0x06U, 1U);
    for (wait = 0U; wait < 256U; ++wait)
    {
        (void)inp(wg_sb16_base + 0x06U);
    }
    outp(wg_sb16_base + 0x06U, 0U);
    return WG_SB16Read(&value) && value == 0xaaU;
}

static int WG_SB16ParseBLASTER(void)
{
    const char *text = getenv("BLASTER");
    int have_base = 0;
    int have_irq = 0;


    wg_sb16_base = 0U;
    wg_sb16_irq = 0U;
    wg_sb16_dma = 0xffU;
    wg_sb16_low_dma = 0xffU;
    wg_sb16_high_dma = 0xffU;
    while (text != NULL && *text != '\0')
    {
        char setting;
        char *end;
        unsigned long value;

        while (*text == ' ' || *text == '\t')
        {
            ++text;
        }
        setting = *text;
        if (setting == '\0')
        {
            break;
        }
        ++text;
        value = strtoul(text, &end,
                        setting == 'A' || setting == 'a' ? 16 : 10);
        if (end == text)
        {
            while (*text != '\0' && *text != ' ' && *text != '\t')
            {
                ++text;
            }
            continue;
        }
        if (setting == 'A' || setting == 'a')
        {
            if (value > 0xffffUL) return 0;
            wg_sb16_base = (uint16_t)value;
            have_base = 1;
        }
        else if (setting == 'I' || setting == 'i')
        {
            if (value > 255UL) return 0;
            wg_sb16_irq = (uint8_t)value;
            have_irq = 1;
        }
        else if (setting == 'D' || setting == 'd')
        {
            if (value > 3UL || value == 2UL) return 0;
            wg_sb16_low_dma = (uint8_t)value;
        }
        else if (setting == 'H' || setting == 'h')
        {
            if (value < 5UL || value > 7UL) return 0;
            wg_sb16_high_dma = (uint8_t)value;
        }
        text = end;
    }
    if (wg_sb16_irq == 2U)
    {
        wg_sb16_irq = 9U;
    }
    return have_base && have_irq
        && wg_sb16_base >= 0x0200U && wg_sb16_base <= 0x0280U
        && (wg_sb16_base & 0x000fU) == 0U
        && (wg_sb16_irq == 5U || wg_sb16_irq == 7U
            || wg_sb16_irq == 9U || wg_sb16_irq == 10U
            || wg_sb16_irq == 11U)
        && (wg_sb16_low_dma != 0xffU || wg_sb16_high_dma != 0xffU);
}

static uint8_t *WG_SB16AllocateDOSBlock(uint32_t bytes,
                                        uint32_t *physical)
{
    union REGS registers;
    uint16_t selector;

    memset(&registers, 0, sizeof(registers));
    registers.w.ax = 0x0100U;
    registers.w.bx = (uint16_t)((bytes + 15U) >> 4);
    (void)int386(0x31, &registers, &registers);
    if (registers.x.cflag != 0U)
    {
        return NULL;
    }
    selector = registers.w.dx;
    if (wg_sb16_dos_block_count >= WG_SB16_MAX_DOS_BLOCKS)
    {
        registers.w.ax = 0x0101U;
        registers.w.dx = selector;
        (void)int386(0x31, &registers, &registers);
        return NULL;
    }
    wg_sb16_dos_selectors[wg_sb16_dos_block_count++] = selector;
    *physical = (registers.x.eax & 0xffffUL) << 4;
    return (uint8_t *)*physical;
}

static int WG_SB16AllocateDMA(void)
{
    uint8_t attempt;

    wg_sb16_dos_block_count = 0U;
    for (attempt = 0U; attempt < WG_SB16_MAX_DOS_BLOCKS; ++attempt)
    {
        uint32_t physical;
        uint8_t *buffer = WG_SB16AllocateDOSBlock(WG_SB16_DMA_BYTES,
                                                   &physical);

        if (buffer == NULL)
        {
            return 0;
        }
        if ((physical & (WG_SB16_DMA_BOUNDARY - 1U))
            + WG_SB16_DMA_BYTES <= WG_SB16_DMA_BOUNDARY)
        {
            wg_sb16_buffer = buffer;
            wg_sb16_buffer_physical = physical;
            memset(wg_sb16_buffer, wg_sb16_8bit ? 0x80 : 0,
                   WG_SB16_DMA_BYTES);
            return 1;
        }
    }
    return 0;
}

static void WG_SB16FreeDMA(void)
{
    while (wg_sb16_dos_block_count != 0U)
    {
        union REGS registers;

        memset(&registers, 0, sizeof(registers));
        registers.w.ax = 0x0101U;
        registers.w.dx =
            wg_sb16_dos_selectors[--wg_sb16_dos_block_count];
        (void)int386(0x31, &registers, &registers);
    }
    wg_sb16_buffer = NULL;
    wg_sb16_buffer_physical = 0U;
}

static uint8_t WG_SB16MixerRead(uint8_t index)
{
    outp(wg_sb16_base + 0x04U, index);
    return (uint8_t)inp(wg_sb16_base + 0x05U);
}

static void __interrupt __far WG_SB16Interrupt(void)
{
    /* DSP 2/3 has no SB16 IRQ-status register: its IRQ must be dedicated. */
    if (wg_sb16_dsp_major >= 4U
        && (WG_SB16MixerRead(0x82U) & (wg_sb16_8bit ? 1U : 2U)) == 0U)
    {
        wg_sb16_old_interrupt();
        return;
    }

    wg_sb16_half_free[wg_sb16_play_half] = 1U;
    wg_sb16_play_half ^= 1U;
    (void)inp(wg_sb16_base + (wg_sb16_8bit ? 0x0eU : 0x0fU));
    if (wg_sb16_irq >= 8U)
    {
        outp(0x00a0U, 0x20U);
    }
    outp(0x0020U, 0x20U);
}

static void WG_SB16ProgramDMA(void)
{
    uint8_t channel = (uint8_t)(wg_sb16_dma - 5U);
    uint32_t word_address = wg_sb16_buffer_physical >> 1;
    uint16_t word_count = (uint16_t)(WG_SB16_DMA_BYTES / 2U - 1U);

    if (wg_sb16_8bit)
    {
        static const uint16_t pages[4] = { 0x87U, 0x83U, 0x81U, 0x82U };
        uint16_t address = (uint16_t)wg_sb16_buffer_physical;
        uint16_t count = (uint16_t)(wg_sb16_half_bytes * 2U - 1U);
        outp(0x0aU, 4U | wg_sb16_dma);
        outp(0x0cU, 0U);
        outp(0x0bU, 0x58U | wg_sb16_dma);
        outp(wg_sb16_dma * 2U, address & 0xffU);
        outp(wg_sb16_dma * 2U, address >> 8);
        outp(pages[wg_sb16_dma], (wg_sb16_buffer_physical >> 16) & 0xffU);
        outp(0x0cU, 0U);
        outp(wg_sb16_dma * 2U + 1U, count & 0xffU);
        outp(wg_sb16_dma * 2U + 1U, count >> 8);
        outp(0x0aU, wg_sb16_dma);
        return;
    }
    outp(0x00d4U, 0x04U | (wg_sb16_dma & 0x03U));
    outp(0x00d8U, 0U);
    outp(0x00d6U, 0x58U | (wg_sb16_dma & 0x03U));
    outp(wg_sb16_dma_address_ports[channel], word_address & 0xffU);
    outp(wg_sb16_dma_address_ports[channel], (word_address >> 8) & 0xffU);
    outp(wg_sb16_dma_page_ports[channel],
         (wg_sb16_buffer_physical >> 16) & 0xffU);
    outp(0x00d8U, 0U);
    outp(wg_sb16_dma_count_ports[channel], word_count & 0xffU);
    outp(wg_sb16_dma_count_ports[channel], word_count >> 8);
    outp(0x00d4U, wg_sb16_dma & 0x03U);
}

static int WG_SB16InstallInterrupt(void)
{
    if (wg_sb16_irq < 8U)
    {
        wg_sb16_vector = (uint8_t)(0x08U + wg_sb16_irq);
    }
    else
    {
        wg_sb16_vector = (uint8_t)(0x70U + wg_sb16_irq - 8U);
    }
    wg_sb16_old_interrupt = _dos_getvect(wg_sb16_vector);
    if (wg_sb16_old_interrupt == NULL)
    {
        return 0;
    }

    wg_sb16_master_mask = (uint8_t)inp(0x0021U);
    wg_sb16_slave_mask = (uint8_t)inp(0x00a1U);
    _disable();
    _dos_setvect(wg_sb16_vector, WG_SB16Interrupt);
    if (wg_sb16_irq >= 8U)
    {
        outp(0x00a1U,
             wg_sb16_slave_mask & ~(1U << (wg_sb16_irq - 8U)));
        outp(0x0021U, wg_sb16_master_mask & ~(1U << 2));
    }
    else
    {
        outp(0x0021U, wg_sb16_master_mask & ~(1U << wg_sb16_irq));
    }
    _enable();
    return 1;
}

int WG_DOSSB16Init(const wolf3d_pcm_format_t *requested,
                   wolf3d_pcm_format_t *obtained)
{
    uint8_t major;
    uint8_t minor;
    uint16_t sample_rate;
    uint16_t block_samples =
        (uint16_t)(WG_SB16_HALF_BYTES / sizeof(int16_t) - 1U);

    WG_DOSSB16Shutdown();
    strcpy(wg_sb16_status, "BLASTER invalid, DSP not responding, or DSP older than 2.0");
    if (requested == NULL || obtained == NULL
        || requested->channels != WG_SB16_CHANNELS
        || requested->bits_per_sample != WG_SB16_BITS
        || !WG_SB16ParseBLASTER() || !WG_SB16ResetDSP()
        || !WG_SB16Write(0xe1U) || !WG_SB16Read(&major)
        || !WG_SB16Read(&minor) || major < 2U)
    {
        return 0;
    }
    wg_sb16_dsp_major = major;
    wg_sb16_8bit = major < 4U || wg_sb16_high_dma == 0xffU;
    wg_sb16_dma = wg_sb16_8bit ? wg_sb16_low_dma : wg_sb16_high_dma;
    if (wg_sb16_dma == 0xffU)
    {
        strcpy(wg_sb16_status, "DSP requires a valid BLASTER D channel");
        return 0;
    }
    wg_sb16_half_bytes = wg_sb16_8bit ? WG_SB16_HALF_FRAMES : WG_SB16_HALF_BYTES;
    sample_rate = requested->sample_rate > 44100U
                    ? 44100U : (uint16_t)requested->sample_rate;
    if (sample_rate < 5000U) sample_rate = 5000U;
    if (wg_sb16_8bit)
    {
        /* Normal-speed mono auto-init works on DSP 2.x and 3.x. Round
           the period UP to stay within 22050 Hz; report the actual rate. */
        unsigned period;
        if (sample_rate > 22050U) sample_rate = 22050U;
        period = (1000000UL + sample_rate - 1U) / sample_rate;
        sample_rate = (uint16_t)(1000000UL / period);
    }
    if (!WG_SB16AllocateDMA() || !WG_SB16InstallInterrupt())
    {
        WG_DOSSB16Shutdown();
        strcpy(wg_sb16_status, "unable to allocate DMA buffer or install IRQ");
        return 0;
    }

    wg_sb16_play_half = 0U;
    wg_sb16_half_free[0] = 0U;
    wg_sb16_half_free[1] = 1U;
    WG_SB16ProgramDMA();
    if (wg_sb16_8bit && major == 3U)
    {
        /* Preserve the SB Pro filter/mode register, but select mono. */
        wg_sb16_mixer_output = WG_SB16MixerRead(0x0eU);
        wg_sb16_mixer_saved = 1U;
        outp(wg_sb16_base + 4U, 0x0eU);
        outp(wg_sb16_base + 5U, wg_sb16_mixer_output & ~2U);
    }
    if (!WG_SB16Write(0xd1U)) goto start_failed;
    if (wg_sb16_8bit)
    {
        unsigned period = (1000000UL + sample_rate / 2U) / sample_rate;
        block_samples = WG_SB16_HALF_FRAMES - 1U;
        if (!WG_SB16Write(0x40U)
            || !WG_SB16Write((uint8_t)(256U - period))
            || !WG_SB16Write(0x48U)
            || !WG_SB16Write((uint8_t)block_samples)
            || !WG_SB16Write((uint8_t)(block_samples >> 8))
            || !WG_SB16Write(0x1cU)) goto start_failed;
    }
    else if (!WG_SB16Write(0x41U)
        || !WG_SB16Write((uint8_t)(sample_rate >> 8))
        || !WG_SB16Write((uint8_t)sample_rate)
        || !WG_SB16Write(0xb6U)
        || !WG_SB16Write(0x30U)
        || !WG_SB16Write((uint8_t)block_samples)
        || !WG_SB16Write((uint8_t)(block_samples >> 8))) goto start_failed;
    sprintf(wg_sb16_status, "DSP %u.%02u: %s, %u Hz, IRQ %u, DMA %u",
            (unsigned)major, (unsigned)minor,
            wg_sb16_8bit ? "8-bit mono" : "16-bit stereo",
            (unsigned)sample_rate, (unsigned)wg_sb16_irq, (unsigned)wg_sb16_dma);
    wg_sb16_active = 1U;
    obtained->sample_rate = sample_rate;
    obtained->channels = WG_SB16_CHANNELS;
    obtained->bits_per_sample = WG_SB16_BITS;
    return 1;

start_failed:
    WG_DOSSB16Shutdown();
    strcpy(wg_sb16_status, "DSP timed out while starting playback");
    return 0;
}

void WG_DOSSB16Shutdown(void)
{
    if (wg_sb16_old_interrupt != NULL)
    {
        (void)WG_SB16Write(0xd3U);
        (void)WG_SB16Write(wg_sb16_8bit ? 0xd0U : 0xd5U);
        (void)WG_SB16ResetDSP();
        if (wg_sb16_mixer_saved)
        {
            outp(wg_sb16_base + 4U, 0x0eU);
            outp(wg_sb16_base + 5U, wg_sb16_mixer_output);
            wg_sb16_mixer_saved = 0U;
        }
    }
    if (wg_sb16_old_interrupt != NULL && wg_sb16_8bit && wg_sb16_dma <= 3U)
    {
        outp(0x0aU, 4U | wg_sb16_dma);
    }
    if (wg_sb16_old_interrupt != NULL && wg_sb16_dma >= 5U && wg_sb16_dma <= 7U)
    {
        outp(0x00d4U, 0x04U | (wg_sb16_dma & 0x03U));
    }
    if (wg_sb16_old_interrupt != NULL)
    {
        _disable();
        outp(0x0021U, wg_sb16_master_mask);
        outp(0x00a1U, wg_sb16_slave_mask);
        _dos_setvect(wg_sb16_vector, wg_sb16_old_interrupt);
        _enable();
    }
    wg_sb16_old_interrupt = NULL;
    wg_sb16_active = 0U;
    WG_SB16FreeDMA();
}

size_t WG_DOSSB16WritableFrames(void)
{
    if (!wg_sb16_active)
    {
        return 0U;
    }
    return wg_sb16_half_free[0] || wg_sb16_half_free[1]
               ? WG_SB16_HALF_FRAMES : 0U;
}

int WG_DOSSB16Submit(const int16_t *samples, size_t frame_count)
{
    uint8_t half;

    if (!wg_sb16_active || samples == NULL
        || frame_count != WG_SB16_HALF_FRAMES)
    {
        return 0;
    }
    _disable();
    if (wg_sb16_half_free[0])
    {
        half = 0U;
    }
    else if (wg_sb16_half_free[1])
    {
        half = 1U;
    }
    else
    {
        _enable();
        return 0;
    }
    if (wg_sb16_8bit)
    {
        size_t index;
        uint8_t *destination = wg_sb16_buffer + (size_t)half * wg_sb16_half_bytes;
        for (index = 0U; index < frame_count; ++index)
        {
            long sum = (long)samples[index * 2U] + samples[index * 2U + 1U];
            destination[index] = (uint8_t)((sum + 65536L) / 512L);
        }
    }
    else
    {
        memcpy(wg_sb16_buffer + (size_t)half * wg_sb16_half_bytes,
               samples, WG_SB16_HALF_BYTES);
    }
    wg_sb16_half_free[half] = 0U;
    _enable();
    return 1;
}
