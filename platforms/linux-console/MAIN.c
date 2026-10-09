#include "WOLF3D.h"
#include "../WG_HOST.h"
#include "WG_LINUX_CONSOLE.h"
#include "../WG_HELP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include "WG_LINUX_IOCTL.h"
#include <unistd.h>

#define WG_DIAG_BITS_PER_LONG (sizeof(unsigned long) * 8U)
#define WG_DIAG_LONGS(maximum) (((maximum) / WG_DIAG_BITS_PER_LONG) + 1U)

static int WG_DiagnosticBit(const unsigned long *bits, unsigned bit)
{
    return (bits[bit / WG_DIAG_BITS_PER_LONG]
            & (1UL << (bit % WG_DIAG_BITS_PER_LONG))) != 0UL;
}

static void WG_LinuxConsoleDiagnosticReport(char *report, size_t report_size)
{
    unsigned drm_count = 0U;
    unsigned input_count = 0U;
    unsigned mouse_count = 0U;
    unsigned joystick_count = 0U;
    unsigned index;
    char path[64];

    for (index = 0U; index < 32U; ++index)
    {
        (void)snprintf(path, sizeof(path), "/dev/dri/card%u", index);
        if (access(path, R_OK | W_OK) == 0)
        {
            ++drm_count;
        }
        (void)snprintf(path, sizeof(path), "/dev/input/event%u", index);
        if (access(path, R_OK) == 0)
        {
            unsigned long events[WG_DIAG_LONGS(EV_MAX)];
            unsigned long keys[WG_DIAG_LONGS(KEY_MAX)];
            unsigned long relative[WG_DIAG_LONGS(REL_MAX)];
            unsigned long absolute[WG_DIAG_LONGS(ABS_MAX)];
            int descriptor = open(path, O_RDONLY | O_NONBLOCK);

            ++input_count;
            memset(events, 0, sizeof(events));
            memset(keys, 0, sizeof(keys));
            memset(relative, 0, sizeof(relative));
            memset(absolute, 0, sizeof(absolute));
            if (descriptor >= 0
                && WG_IOCTL(descriptor, EVIOCGBIT(0, sizeof(events)), events) >= 0)
            {
                if (WG_DiagnosticBit(events, EV_KEY))
                {
                    (void)WG_IOCTL(descriptor,
                        EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
                }
                if (WG_DiagnosticBit(events, EV_REL))
                {
                    (void)WG_IOCTL(descriptor,
                        EVIOCGBIT(EV_REL, sizeof(relative)), relative);
                }
                if (WG_DiagnosticBit(events, EV_ABS))
                {
                    (void)WG_IOCTL(descriptor,
                        EVIOCGBIT(EV_ABS, sizeof(absolute)), absolute);
                }
                if (WG_DiagnosticBit(relative, REL_X)
                    && WG_DiagnosticBit(relative, REL_Y)
                    && WG_DiagnosticBit(keys, BTN_MOUSE))
                {
                    ++mouse_count;
                }
                if (WG_DiagnosticBit(absolute, ABS_X)
                    && WG_DiagnosticBit(absolute, ABS_Y)
                    && (WG_DiagnosticBit(keys, BTN_JOYSTICK)
                        || WG_DiagnosticBit(keys, BTN_GAMEPAD)))
                {
                    ++joystick_count;
                }
            }
            if (descriptor >= 0)
            {
                (void)close(descriptor);
            }
        }
    }
    (void)snprintf(report, report_size,
        "  DRM/KMS: %u accessible card device(s)\n"
        "  fbdev: %s\n"
        "  ALSA: %s\n"
        "  evdev: %u readable device(s)\n"
        "  Mouse: %u detected evdev device(s)\n"
        "  Joystick/gamepad: %u detected evdev device(s)\n",
        drm_count, access("/dev/fb0", R_OK | W_OK) == 0
                       ? "/dev/fb0 accessible" : "no accessible /dev/fb0",
        access("/dev/snd", R_OK) == 0 ? "/dev/snd accessible"
                                       : "no accessible /dev/snd",
        input_count, mouse_count, joystick_count);
}

static void WG_PrintHelp(const char *program)
{
    WG_PrintCommandLineHelp(
        program != NULL ? program : "wolf3d",
        "Linux console host options:\n"
        "  --video MODE         Video backend: auto, drm, or fbdev (default: auto)\n"
        "  --drm-device PATH    DRM card to use (default: first usable card)\n"
        "  --fb-device PATH     Framebuffer (default: $FRAMEBUFFER or /dev/fb0)\n"
        "  --input-device PATH  evdev device (repeatable; default: auto)\n"
        "  --alsa-device NAME   ALSA PCM device (default: default)\n"
        "  --no-audio           Run without opening an ALSA device\n"
        "  --linux-console-help Alias for --help\n",
        "Run from an active virtual console with access to the selected video,\n"
        "evdev input, and ALSA devices. DRM/KMS is preferred over fbdev.\n");
}

int main(int argc, char **argv)
{
    char **game_argv;
    int game_argc = 0;
    int index;
    int success = 1;
    char launcher_error[2048];
    wg_launcher_arguments_t launcher_arguments;
    wg_game_arguments_t prepared;
    wolf3d_result_t result;

    if (!WG_LoadLauncherArguments(argc, argv, &launcher_arguments,
                                  launcher_error, sizeof(launcher_error)))
    {
        fprintf(stderr, "wolf3d: %s", launcher_error);
        return 1;
    }
    argc = launcher_arguments.argc;
    argv = launcher_arguments.argv;
    game_argv = (char **)calloc((size_t)argc + 1U, sizeof(*game_argv));
    if (game_argv == NULL)
    {
        fprintf(stderr, "wolf3d: unable to allocate arguments.\n");
        WG_FreeLauncherArguments(&launcher_arguments);
        return 1;
    }
    game_argv[game_argc++] = argv[0];
    for (index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--linux-console-help") == 0
            || WG_CommandLineHelpRequested(argc, argv))
        {
            WG_PrintHelp(argv[0]);
            free(game_argv);
            WG_FreeLauncherArguments(&launcher_arguments);
            return 0;
        }
        if (strcmp(argv[index], "--no-audio") == 0)
        {
            WG_LinuxConsoleDisableAudio();
            continue;
        }
        if (strcmp(argv[index], "--video") == 0
            || strcmp(argv[index], "--drm-device") == 0
            || strcmp(argv[index], "--fb-device") == 0
            || strcmp(argv[index], "--input-device") == 0
            || strcmp(argv[index], "--alsa-device") == 0)
        {
            const char *option = argv[index];

            if (++index >= argc)
            {
                fprintf(stderr, "wolf3d: %s requires a value.\n",
                        option);
                success = 0;
                break;
            }
            if (strcmp(option, "--video") == 0)
            {
                success = WG_LinuxConsoleSetVideoBackend(argv[index]);
            }
            else if (strcmp(option, "--drm-device") == 0)
            {
                success = WG_LinuxConsoleSetDRMDevice(argv[index]);
            }
            else if (strcmp(option, "--fb-device") == 0)
            {
                success = WG_LinuxConsoleSetFramebufferDevice(argv[index]);
            }
            else if (strcmp(option, "--input-device") == 0)
            {
                success = WG_LinuxConsoleAddInputDevice(argv[index]);
            }
            else
            {
                success = WG_LinuxConsoleSetALSADevice(argv[index]);
            }
            if (!success)
            {
                fprintf(stderr, "wolf3d: invalid value for %s.\n",
                        option);
                break;
            }
            continue;
        }
        game_argv[game_argc++] = argv[index];
    }

    if (success && WG_CommandLineDiagnosticsRequested(argc, argv))
    {
        char report[1024];

        WG_LinuxConsoleDiagnosticReport(report, sizeof(report));
        WG_PrintDiagnostics(argc, argv, launcher_arguments.config_path,
                            report);
        free(game_argv);
        WG_FreeLauncherArguments(&launcher_arguments);
        return 0;
    }

    if (success
        && !WG_PrepareGameArguments(game_argc, game_argv, &prepared,
                                    launcher_error,
                                    sizeof(launcher_error)))
    {
        fprintf(stderr, "wolf3d: %s", launcher_error);
        success = 0;
    }
    if (!success)
    {
        free(game_argv);
        WG_FreeLauncherArguments(&launcher_arguments);
        return 1;
    }
    if (!WG_InstallPlatform())
    {
        fprintf(stderr, "wolf3d: unable to install platform API.\n");
        WG_FreeGameArguments(&prepared);
        free(game_argv);
        WG_FreeLauncherArguments(&launcher_arguments);
        return 1;
    }

    result = wolf3d_Create(prepared.argc, prepared.argv);
    WG_FreeGameArguments(&prepared);
    if (result == WOLF3D_RESULT_OK)
    {
        result = wolf3d_Run();
        wolf3d_Shutdown();
    }
    free(game_argv);
    WG_FreeLauncherArguments(&launcher_arguments);
    return result == WOLF3D_RESULT_QUIT || result == WOLF3D_RESULT_NOT_IMPLEMENTED
               ? 0 : 1;
}
