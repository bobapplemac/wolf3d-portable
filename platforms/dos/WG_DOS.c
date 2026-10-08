#include "WOLF3D.h"
#include "../WG_HELP.h"
#include "WG_DOS_ADLIB.h"
#include "WG_DOS_SB16.h"

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
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
static uint8_t wg_null_pcm;
static uint32_t wg_null_pcm_rate;
static uint32_t wg_null_pcm_ready_at;

static int WG_DOSPCMInitEx(const wolf3d_pcm_format_t *requested,
                           wolf3d_pcm_format_t *obtained);

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
    WG_DOSSB16Shutdown();
    wg_null_pcm = 0U;
    wg_null_pcm_rate = 0U;
    wg_null_pcm_ready_at = 0U;
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
    uint16_t content_rows = 0U;
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
        uint16_t column;

        memcpy(WG_DOS_TEXT_MEMORY
                   + (size_t)row * WOLF3D_TEXT_COLUMNS
                     * WOLF3D_TEXT_CELL_BYTES,
               cells + (size_t)row * columns * WOLF3D_TEXT_CELL_BYTES,
               (size_t)columns * WOLF3D_TEXT_CELL_BYTES);
        for (column = 0U; column < columns; ++column)
        {
            uint8_t character =
                cells[((size_t)row * columns + column)
                      * WOLF3D_TEXT_CELL_BYTES];

            if (character != 0U && character != ' ')
            {
                content_rows = (uint16_t)(row + 1U);
                break;
            }
        }
    }
    /* COMMAND.COM advances once before printing its prompt. Start on the
       final content row so the prompt follows screens of every height, while
       reserving the last row to prevent a full-page scroll. */
    row = content_rows != 0U ? (uint16_t)(content_rows - 1U) : 0U;
    if (row >= WOLF3D_TEXT_ROWS - 1U)
    {
        row = WOLF3D_TEXT_ROWS - 2U;
    }
    WG_DOSSetCursor((uint8_t)row, 0U);
    wg_text_presented = 1U;
    wg_graphics_active = 0U;
}

static int WG_DOSPCMInit(uint32_t sample_rate, uint16_t channels)
{
    wolf3d_pcm_format_t requested;
    wolf3d_pcm_format_t obtained;

    requested.sample_rate = sample_rate;
    requested.channels = channels;
    requested.bits_per_sample = 16U;
    return WG_DOSPCMInitEx(&requested, &obtained);
}

static int WG_DOSPCMInitEx(const wolf3d_pcm_format_t *requested,
                           wolf3d_pcm_format_t *obtained)
{
    if (requested == NULL || obtained == NULL
        || requested->sample_rate == 0U || requested->channels != 2U
        || requested->bits_per_sample != 16U)
    {
        return 0;
    }
    WG_DOSSB16Shutdown();
    wg_null_pcm = 0U;
    if (WG_DOSSB16Init(requested, obtained))
    {
        return 1;
    }
    *obtained = *requested;
    wg_null_pcm = 1U;
    wg_null_pcm_rate = requested->sample_rate;
    wg_null_pcm_ready_at = WG_DOSGetTicksMs();
    return 1;
}

static void WG_DOSPCMShutdown(void)
{
    WG_DOSSB16Shutdown();
    wg_null_pcm = 0U;
    wg_null_pcm_rate = 0U;
    wg_null_pcm_ready_at = 0U;
}

static size_t WG_DOSPCMWritableFrames(void)
{
    if (wg_null_pcm)
    {
        return (int32_t)(WG_DOSGetTicksMs() - wg_null_pcm_ready_at) >= 0
                   ? 1024U : 0U;
    }
    return WG_DOSSB16WritableFrames();
}

static int WG_DOSPCMSubmit(const int16_t *samples, size_t frame_count)
{
    if (wg_null_pcm)
    {
        if (samples == NULL || frame_count == 0U || wg_null_pcm_rate == 0U)
        {
            return 0;
        }
        wg_null_pcm_ready_at = WG_DOSGetTicksMs()
            + (uint32_t)((frame_count * 1000U + wg_null_pcm_rate - 1U)
                         / wg_null_pcm_rate);
        return 1;
    }
    return WG_DOSSB16Submit(samples, frame_count);
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
        WG_DOSPCMInitEx,
        WG_DOSAdLibInit,
        WG_DOSAdLibShutdown,
        WG_DOSAdLibWrite,
        NULL,
        NULL
    };

    return wolf3d_SetPlatform(&platform) == WOLF3D_RESULT_OK;
}

static void WG_DOSDiagnosticReport(char *report, size_t report_size)
{
    union REGS registers;
    const char *blaster = getenv("BLASTER");
    int adlib;

    memset(&registers, 0, sizeof(registers));
    registers.w.ax = 0U;
    (void)int386(0x33, &registers, &registers);
    adlib = WG_DOSAdLibInit();
    if (adlib)
    {
        WG_DOSAdLibShutdown();
    }
    (void)sprintf(report,
        "  Video: VGA BIOS mode 13h output\n"
        "  AdLib: %s\n"
        "  Sound Blaster: %s\n"
        "  Mouse driver: %s\n"
        "  Joystick: not implemented by the DOS host\n",
        adlib ? "detected" : "not detected",
        blaster != NULL && blaster[0] != '\0' ? blaster
                                               : "BLASTER is not configured",
        registers.w.ax != 0U ? "present" : "not detected");
    (void)report_size;
}

int main(int argc, char **argv)
{
    char launcher_error[2048];
    wg_launcher_arguments_t launcher_arguments;
    wg_game_arguments_t game_arguments;
    wolf3d_result_t result;

    if (!WG_LoadLauncherArguments(argc, argv, &launcher_arguments,
                                  launcher_error, sizeof(launcher_error)))
    {
        WG_PrintLauncherError(launcher_error);
        return 1;
    }
    argc = launcher_arguments.argc;
    argv = launcher_arguments.argv;
    if (WG_CommandLineHelpRequested(argc, argv))
    {
        WG_PrintCommandLineHelp(
            argc > 0 ? argv[0] : "WOLF3D.EXE", NULL,
            "The DOS host defaults to native AdLib OPL and SB16 PCM output.\n");
        WG_FreeLauncherArguments(&launcher_arguments);
        return 0;
    }
    if (WG_CommandLineDiagnosticsRequested(argc, argv))
    {
        char report[1024];

        WG_DOSDiagnosticReport(report, sizeof(report));
        WG_PrintDiagnostics(argc, argv, launcher_arguments.config_path,
                            report);
        WG_FreeLauncherArguments(&launcher_arguments);
        return 0;
    }
    if (!WG_DOSInstallPlatform())
    {
        WG_FreeLauncherArguments(&launcher_arguments);
        return 1;
    }
    if (!WG_PrepareGameArguments(argc, argv, &game_arguments,
                                 launcher_error, sizeof(launcher_error)))
    {
        WG_PrintLauncherError(launcher_error);
        WG_FreeLauncherArguments(&launcher_arguments);
        return 1;
    }
    result = wolf3d_Create(game_arguments.argc, game_arguments.argv);
    WG_FreeGameArguments(&game_arguments);
    WG_FreeLauncherArguments(&launcher_arguments);
    if (result == WOLF3D_RESULT_OK)
    {
        result = wolf3d_Run();
        wolf3d_Shutdown();
    }
    return result == WOLF3D_RESULT_QUIT
        || result == WOLF3D_RESULT_NOT_IMPLEMENTED ? 0 : 1;
}
