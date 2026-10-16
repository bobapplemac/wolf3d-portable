/* Shared runtime selection. Backend initialization may fall back; game errors
   never do. SDL libraries are optional even when their host is compiled. */
#include <stdio.h>
#include "WG_PRINTF_COMPAT.h"
#include <stdlib.h>
#include <string.h>
#include "WG_HELP.h"
#include "win32/WG_BACKEND.h"
#if defined(WG_HAVE_SDL1) && defined(WG_HAVE_SDL3)
#error SDL1 and SDL3 cannot be included in the same build
#endif
#ifdef WG_HAVE_GDI
int WG_GDIRun(int argc, char **argv);
#endif
#ifdef WG_HAVE_KMS
int WG_KMSRun(int argc, char **argv);
#endif
typedef struct wg_backend_entry {
    const char *name;
    int (*load)(char *, size_t);
    int (*run)(int, char **);
    void (*unload)(void);
} wg_backend_entry;
static const wg_backend_entry backends[] = {
#ifdef WG_HAVE_SDL3
    {"sdl3", WG_SDL3Load, WG_SDL3Run, WG_SDL3Unload},
#endif
#ifdef WG_HAVE_SDL1
    {"sdl1", WG_SDL1Load, WG_SDL1Run, WG_SDL1Unload},
#endif
#ifdef WG_HAVE_GDI
    {"gdi", NULL, WG_GDIRun, NULL},
#endif
#ifdef WG_HAVE_KMS
    {"kms-fbdev", NULL, WG_KMSRun, NULL},
#endif
    {NULL, NULL, NULL, NULL}
};

int WG_LaunchBackends(int argc, char **argv, int *no_pause)
{
    wg_launcher_arguments_t options;
    char error[2048];
    static char compiled[80];
    const char *choice = "auto";
    char **effective;
    int i, count = 0, result = 1, automatic, known = 0;
    const wg_backend_entry *backend;
    if (!WG_LoadLauncherArguments(argc, argv, &options, error, sizeof(error))) {
        WG_PrintLauncherError(error); return 1;
    }
    effective = (char **)calloc((size_t)options.argc + 2, sizeof(*effective));
    if (!effective) { WG_FreeLauncherArguments(&options); return 1; }
    effective[count++] = options.argv[0];
    for (i = 1; i < options.argc; ++i) {
        if (!strcmp(options.argv[i], "--backend")) {
            if (++i == options.argc) {
                WG_PrintLauncherError("--backend requires auto or a compiled backend name.\n");
                goto done;
            }
            choice = options.argv[i];
        } else if (strcmp(options.argv[i], "--no-config")) {
            effective[count++] = options.argv[i];
            if (!strcmp(options.argv[i], "--no-pause")) *no_pause = 1;
        }
    }
    effective[count++] = "--no-config";
    WG_SetLauncherConfigPath(options.config_path);
    automatic = !strcmp(choice, "auto");
    compiled[0] = '\0';
    for (backend = backends; backend->name; ++backend) {
        if (compiled[0]) strcat(compiled, " ");
        strcat(compiled, backend->name);
        if (!strcmp(choice, backend->name)) known = 1;
    }
    WG_SetVideoInfo(compiled, choice, NULL);
    if (!automatic && !known) {
        snprintf(error, sizeof(error), "Backend '%s' is not compiled into this package. Available: %s\n", choice, compiled);
        WG_PrintLauncherError(error); goto done;
    }
    if (WG_CommandLineHelpRequested(count, effective)) {
        WG_PrintCommandLineHelp(effective[0], NULL, NULL);
        result = 0; goto done;
    }
    for (backend = backends; backend->name; ++backend) {
        if (!automatic && strcmp(choice, backend->name)) continue;
        if (!WG_ValidateBackendOptions(count, effective, backend->name)) goto done;
        if (backend->load && !backend->load(error, sizeof(error))) {
            if (!automatic) { WG_PrintLauncherError(error); goto done; }
            if (WG_CommandLineDiagnosticsRequested(count, effective))
                printf("%s unavailable: %s\n", backend->name, error);
            continue;
        }
        WG_SetVideoInfo(NULL, backend->name, NULL);
        result = backend->run(count, effective);
        if (backend->unload) backend->unload();
        if (result != WG_BACKEND_UNAVAILABLE) goto done;
        result = 1;
        if (!automatic) {
            WG_PrintLauncherError("Requested video backend could not initialize.\n");
            goto done;
        }
    }
    WG_PrintLauncherError("No usable compiled video backend is available.\n");
done:
    free(effective);
    WG_FreeLauncherArguments(&options);
    return result;
}
