#include "WOLF3D.h"
#include "../WG_HOST.h"
#include "WG_LINUX_CONSOLE.h"
#include "../WG_HELP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    wg_game_arguments_t prepared;
    wolf3d_result_t result;

    game_argv = (char **)calloc((size_t)argc + 1U, sizeof(*game_argv));
    if (game_argv == NULL)
    {
        fprintf(stderr, "wolf3d: unable to allocate arguments.\n");
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
        return 1;
    }
    if (!WG_InstallPlatform())
    {
        fprintf(stderr, "wolf3d: unable to install platform API.\n");
        WG_FreeGameArguments(&prepared);
        free(game_argv);
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
    return result == WOLF3D_RESULT_QUIT || result == WOLF3D_RESULT_NOT_IMPLEMENTED
               ? 0 : 1;
}
