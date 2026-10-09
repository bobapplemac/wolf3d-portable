#include "../platforms/WG_HELP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    wg_game_arguments_t prepared;
    wg_launcher_arguments_t launcher;
    char error[4096];
    char **game_argv;
    char *program;
    size_t root_length;
    size_t name_length;
    int game_argc;
    int index;

    if (argc < 3)
    {
        return 3;
    }
    root_length = strlen(argv[1]);
    name_length = strlen(argv[2]);
    program = (char *)malloc(root_length + name_length + 2U);
    game_argc = argc - 2;
    game_argv = (char **)malloc(((size_t)game_argc + 1U)
                                * sizeof(*game_argv));
    if (program == NULL || game_argv == NULL)
    {
        free(program);
        free(game_argv);
        return 3;
    }
    memcpy(program, argv[1], root_length);
    program[root_length] = '/';
    memcpy(program + root_length + 1U, argv[2], name_length + 1U);
    game_argv[0] = program;
    for (index = 1; index < game_argc; ++index)
    {
        game_argv[index] = argv[index + 2];
    }
    game_argv[game_argc] = NULL;

    if (!WG_LoadLauncherArguments(game_argc, game_argv, &launcher,
                                  error, sizeof(error)))
    {
        (void)fputs(error, stderr);
        free(game_argv);
        free(program);
        return 2;
    }
    if (WG_CommandLineDiagnosticsRequested(launcher.argc, launcher.argv))
    {
        for (index = 1; index < launcher.argc; ++index)
        {
            if (strcmp(launcher.argv[index], "--diag") != 0)
            {
                (void)printf("%s\n", launcher.argv[index]);
            }
        }
        WG_FreeLauncherArguments(&launcher);
        free(game_argv);
        free(program);
        return 0;
    }
    if (!WG_PrepareGameArguments(launcher.argc, launcher.argv, &prepared,
                                 error, sizeof(error)))
    {
        (void)fputs(error, stderr);
        WG_FreeLauncherArguments(&launcher);
        free(game_argv);
        free(program);
        return 2;
    }
    if (prepared.allocated)
    {
        (void)printf("%s\n%s\n", prepared.argv[prepared.argc - 3],
                     prepared.argv[prepared.argc - 1]);
    }
    else
    {
        const char *data = NULL;
        const char *game = NULL;

        for (index = 1; index + 1 < prepared.argc; ++index)
        {
            if (strcmp(prepared.argv[index], "--data") == 0)
            {
                data = prepared.argv[index + 1];
            }
            else if (strcmp(prepared.argv[index], "--game") == 0)
            {
                game = prepared.argv[index + 1];
            }
        }
        if (data != NULL && game != NULL)
        {
            (void)printf("%s\n%s\n", data, game);
        }
        else
        {
            (void)puts("NONE");
        }
    }
    WG_FreeGameArguments(&prepared);
    WG_FreeLauncherArguments(&launcher);
    free(game_argv);
    free(program);
    return 0;
}
