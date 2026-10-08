#ifndef WG_HELP_H
#define WG_HELP_H

#include <stddef.h>

typedef struct wg_game_arguments
{
    int argc;
    char **argv;
    int allocated;
} wg_game_arguments_t;

typedef struct wg_launcher_arguments
{
    int argc;
    char **argv;
    int config_argc;
    char *config_path;
} wg_launcher_arguments_t;

int WG_CommandLineHelpRequested(int argc, char **argv);
int WG_CommandLineDiagnosticsRequested(int argc, char **argv);
void WG_PrintCommandLineHelp(const char *program,
                             const char *host_options,
                             const char *runtime_notes);
int WG_LoadLauncherArguments(int argc, char **argv,
                             wg_launcher_arguments_t *prepared,
                             char *error, size_t error_size);
void WG_FreeLauncherArguments(wg_launcher_arguments_t *prepared);
void WG_PrintDiagnostics(int argc, char **argv,
                         const char *config_path,
                         const char *host_report);
int WG_PrepareGameArguments(int argc, char **argv,
                            wg_game_arguments_t *prepared,
                            char *error, size_t error_size);
void WG_FreeGameArguments(wg_game_arguments_t *prepared);
void WG_PrintLauncherError(const char *message);

#endif
