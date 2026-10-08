#ifndef WG_HELP_H
#define WG_HELP_H

int WG_CommandLineHelpRequested(int argc, char **argv);
void WG_PrintCommandLineHelp(const char *program,
                             const char *host_options,
                             const char *runtime_notes);

#endif
