#include "../platforms/WG_HELP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    wg_game_arguments_t prepared;
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

    if (!WG_PrepareGameArguments(game_argc, game_argv, &prepared,
                                 error, sizeof(error)))
    {
        (void)fputs(error, stderr);
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
        (void)puts("NONE");
    }
    WG_FreeGameArguments(&prepared);
    free(game_argv);
    free(program);
    return 0;
}
