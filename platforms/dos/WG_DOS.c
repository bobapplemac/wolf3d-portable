#include "WOLF3D.h"

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <string.h>

#define WG_DOS_TIMER_HZ 700U
#define WG_DOS_PIT_FREQUENCY 1192030UL
#define WG_DOS_TIMER_DIVISOR ((uint16_t)(WG_DOS_PIT_FREQUENCY / WG_DOS_TIMER_HZ))
#define WG_DOS_KEY_QUEUE_SIZE 64U
#define WG_DOS_KEY_RELEASE 0x0100U
#define WG_DOS_VGA_MEMORY ((uint8_t *)0x000a0000UL)
#define WG_DOS_TEXT_MEMORY ((uint8_t *)0x000b8000UL)

typedef void (__interrupt __far *wg_dos_interrupt_t)(void);

static wg_dos_interrupt_t wg_old_timer_interrupt;
static wg_dos_interrupt_t wg_old_keyboard_interrupt;
static volatile uint32_t wg_timer_ticks;
static volatile uint16_t wg_timer_fraction;
static volatile uint16_t wg_key_queue[WG_DOS_KEY_QUEUE_SIZE];
static volatile uint8_t wg_key_read;
static volatile uint8_t wg_key_write;
static uint8_t wg_keyboard_e0;
static uint8_t wg_keyboard_pause_bytes;
static uint8_t wg_old_video_mode = 3U;
static uint8_t wg_graphics_active;
static uint8_t wg_text_presented;
static uint8_t wg_palette_cache[WOLF3D_PALETTE_COLORS * 3U];
static uint8_t wg_palette_valid;

static void WG_DOSSetVideoMode(uint8_t mode)
{
    union REGS registers;

    memset(&registers, 0, sizeof(registers));
    registers.w.ax = mode;
    (void)int386(0x10, &registers, &registers);
}

static uint8_t WG_DOSGetVideoMode(void)
{
    union REGS registers;

    memset(&registers, 0, sizeof(registers));
    registers.h.ah = 0x0fU;
    (void)int386(0x10, &registers, &registers);
    return registers.h.al;
}

static void WG_DOSSetCursor(uint8_t row, uint8_t column)
{
    union REGS registers;

    memset(&registers, 0, sizeof(registers));
    registers.h.ah = 0x02U;
    registers.h.bh = 0U;
    registers.h.dh = row;
    registers.h.dl = column;
    (void)int386(0x10, &registers, &registers);
}

static void WG_DOSProgramTimer(uint16_t divisor)
{
    outp(0x43, 0x36);
    outp(0x40, divisor & 0xffU);
    outp(0x40, divisor >> 8);
}

static void WG_DOSQueueKey(uint16_t value)
{
    uint8_t next = (uint8_t)((wg_key_write + 1U)
                             & (WG_DOS_KEY_QUEUE_SIZE - 1U));

    if (next != wg_key_read)
    {
        wg_key_queue[wg_key_write] = value;
        wg_key_write = next;
    }
}

static void __interrupt __far WG_DOSTimerInterrupt(void)
{
    uint16_t previous = wg_timer_fraction;

    ++wg_timer_ticks;
    wg_timer_fraction = (uint16_t)(previous + WG_DOS_TIMER_DIVISOR);
    if (wg_timer_fraction < previous)
    {
        wg_old_timer_interrupt();
    }
    else
    {
        outp(0x20, 0x20);
    }
}

static void __interrupt __far WG_DOSKeyboardInterrupt(void)
{
    uint8_t scan = (uint8_t)inp(0x60);
    uint8_t control = (uint8_t)inp(0x61);

    outp(0x61, control | 0x80U);
    outp(0x61, control);

    if (wg_keyboard_pause_bytes != 0U)
    {
        --wg_keyboard_pause_bytes;
    }
    else if (scan == 0xe0U)
    {
        wg_keyboard_e0 = 1U;
    }
    else if (scan == 0xe1U)
    {
        wg_keyboard_e0 = 0U;
        wg_keyboard_pause_bytes = 5U;
        WG_DOSQueueKey((uint16_t)WOLF3D_KEY_PAUSE);
        WG_DOSQueueKey((uint16_t)WOLF3D_KEY_PAUSE | WG_DOS_KEY_RELEASE);
    }
    else
    {
        uint16_t key = (uint16_t)(scan & 0x7fU);

        (void)wg_keyboard_e0;
        wg_keyboard_e0 = 0U;
        if ((scan & 0x80U) != 0U)
        {
            key |= WG_DOS_KEY_RELEASE;
        }
        WG_DOSQueueKey(key);
    }
    outp(0x20, 0x20);
}

static int WG_DOSInit(void)
{
    wg_old_video_mode = WG_DOSGetVideoMode();
    wg_old_timer_interrupt = _dos_getvect(8);
    wg_old_keyboard_interrupt = _dos_getvect(9);
    wg_timer_ticks = 0U;
    wg_timer_fraction = 0U;
    wg_key_read = 0U;
    wg_key_write = 0U;
    wg_keyboard_e0 = 0U;
    wg_keyboard_pause_bytes = 0U;
    wg_text_presented = 0U;
    wg_palette_valid = 0U;

    _disable();
    _dos_setvect(8, WG_DOSTimerInterrupt);
    _dos_setvect(9, WG_DOSKeyboardInterrupt);
    WG_DOSProgramTimer(WG_DOS_TIMER_DIVISOR);
    _enable();

    WG_DOSSetVideoMode(0x13U);
    wg_graphics_active = 1U;
    return 1;
}

static void WG_DOSShutdown(void)
{
    _disable();
    WG_DOSProgramTimer(0U);
    if (wg_old_timer_interrupt != NULL)
    {
        _dos_setvect(8, wg_old_timer_interrupt);
    }
    if (wg_old_keyboard_interrupt != NULL)
    {
        _dos_setvect(9, wg_old_keyboard_interrupt);
    }
    _enable();

    if (!wg_text_presented)
    {
        WG_DOSSetVideoMode(wg_old_video_mode);
    }
    wg_graphics_active = 0U;
}

static void WG_DOSPresent(const uint8_t *pixels, const uint8_t *palette)
{
    unsigned wait;

    if (!wg_graphics_active || pixels == NULL || palette == NULL)
    {
        return;
    }
    if (!wg_palette_valid
        || memcmp(wg_palette_cache, palette, sizeof(wg_palette_cache)) != 0)
    {
        size_t index;

        outp(0x3c8, 0U);
        for (index = 0U; index < sizeof(wg_palette_cache); ++index)
        {
            outp(0x3c9, ((unsigned)palette[index] * 63U + 127U) / 255U);
        }
        memcpy(wg_palette_cache, palette, sizeof(wg_palette_cache));
        wg_palette_valid = 1U;
    }

    wait = 65535U;
    while ((inp(0x3da) & 0x08U) != 0U && wait-- != 0U)
    {
    }
    wait = 65535U;
    while ((inp(0x3da) & 0x08U) == 0U && wait-- != 0U)
    {
    }
    memcpy(WG_DOS_VGA_MEMORY, pixels,
           WOLF3D_SCREEN_WIDTH * WOLF3D_SCREEN_HEIGHT);
}

static uint32_t WG_DOSGetTicksMs(void)
{
    uint32_t ticks;

    _disable();
    ticks = wg_timer_ticks;
    _enable();
    return (ticks / 7U) * 10U + ((ticks % 7U) * 10U) / 7U;
}

static void WG_DOSSleepMs(uint32_t milliseconds)
{
    uint32_t start = WG_DOSGetTicksMs();

    while ((uint32_t)(WG_DOSGetTicksMs() - start) < milliseconds)
    {
    }
}

static int WG_DOSPollEvent(wolf3d_event_t *event)
{
    uint16_t key;

    if (event == NULL)
    {
        return 0;
    }
    _disable();
    if (wg_key_read == wg_key_write)
    {
        _enable();
        event->type = WOLF3D_EVENT_NONE;
        return 0;
    }
    key = wg_key_queue[wg_key_read];
    wg_key_read = (uint8_t)((wg_key_read + 1U)
                            & (WG_DOS_KEY_QUEUE_SIZE - 1U));
    _enable();

    memset(event, 0, sizeof(*event));
    event->type = WOLF3D_EVENT_KEY;
    event->pressed = (key & WG_DOS_KEY_RELEASE) == 0U;
    event->key = key & 0xffU;
    return 1;
}

static int WG_DOSIsInteractive(void)
{
    return 1;
}

static void WG_DOSSetWindowTitle(const char *title)
{
    (void)title;
}

static void WG_DOSPrintMessage(const char *message)
{
    fprintf(stdout, "%s\n", message);
    fflush(stdout);
}

static void WG_DOSReportError(const char *message)
{
    if (wg_graphics_active)
    {
        WG_DOSSetVideoMode(3U);
        wg_graphics_active = 0U;
    }
    fprintf(stderr, "Wolf3D: %s\n", message);
    fflush(stderr);
}

static void WG_DOSPresentText(const uint8_t *cells, uint16_t columns,
                              uint16_t rows)
{
    uint16_t row;

    if (cells == NULL || columns > WOLF3D_TEXT_COLUMNS
        || rows > WOLF3D_TEXT_ROWS)
    {
        return;
    }
    WG_DOSSetVideoMode(3U);
    memset(WG_DOS_TEXT_MEMORY, 0,
           WOLF3D_TEXT_COLUMNS * WOLF3D_TEXT_ROWS
           * WOLF3D_TEXT_CELL_BYTES);
    for (row = 0U; row < rows; ++row)
    {
        memcpy(WG_DOS_TEXT_MEMORY
                   + (size_t)row * WOLF3D_TEXT_COLUMNS
                     * WOLF3D_TEXT_CELL_BYTES,
               cells + (size_t)row * columns * WOLF3D_TEXT_CELL_BYTES,
               (size_t)columns * WOLF3D_TEXT_CELL_BYTES);
    }
    WG_DOSSetCursor((uint8_t)(WOLF3D_TEXT_ROWS - 1U), 0U);
    wg_text_presented = 1U;
    wg_graphics_active = 0U;
}

static int WG_DOSPCMInit(uint32_t sample_rate, uint16_t channels)
{
    (void)sample_rate;
    (void)channels;
    return 0;
}

static int WG_DOSPCMInitEx(const wolf3d_pcm_format_t *requested,
                           wolf3d_pcm_format_t *obtained)
{
    (void)requested;
    (void)obtained;
    return 0;
}

static void WG_DOSPCMShutdown(void)
{
}

static size_t WG_DOSPCMWritableFrames(void)
{
    return 0U;
}

static int WG_DOSPCMSubmit(const int16_t *samples, size_t frame_count)
{
    (void)samples;
    (void)frame_count;
    return 0;
}

static int WG_DOSInstallPlatform(void)
{
    static const wolf3d_platform_api_t platform =
    {
        WOLF3D_PLATFORM_API_VERSION,
        sizeof(wolf3d_platform_api_t),
        WG_DOSInit,
        WG_DOSShutdown,
        WG_DOSPresent,
        WG_DOSGetTicksMs,
        WG_DOSSleepMs,
        WG_DOSPollEvent,
        WG_DOSIsInteractive,
        WG_DOSSetWindowTitle,
        WG_DOSPrintMessage,
        WG_DOSReportError,
        WG_DOSPresentText,
        WG_DOSPCMInit,
        WG_DOSPCMShutdown,
        WG_DOSPCMWritableFrames,
        WG_DOSPCMSubmit,
        WG_DOSPCMInitEx
    };

    return wolf3d_SetPlatform(&platform) == WOLF3D_RESULT_OK;
}

int main(int argc, char **argv)
{
    wolf3d_result_t result;

    if (!WG_DOSInstallPlatform())
    {
        return 1;
    }
    result = wolf3d_Create(argc, argv);
    if (result == WOLF3D_RESULT_OK)
    {
        result = wolf3d_Run();
        wolf3d_Shutdown();
    }
    return result == WOLF3D_RESULT_QUIT
        || result == WOLF3D_RESULT_NOT_IMPLEMENTED ? 0 : 1;
}
