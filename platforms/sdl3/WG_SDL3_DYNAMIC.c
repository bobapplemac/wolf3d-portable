#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Resolve only the SDL API this host uses; no mandatory SDL import. */
#define SDL_MAIN_HANDLED
#define WG_SDL3_NO_ALIASES
#include "WG_SDL3_DYNAMIC.h"
#ifdef __GNUC__
/* Catch signature drift against whichever supported SDL headers are selected. */
#define WG_SDL3_FUNCTION(result,name,args) \
    typedef char wg_check_##name[__builtin_types_compatible_p(__typeof__(wg_##name), __typeof__(&name)) ? 1 : -1];
#include "WG_SDL3_FUNCTIONS.inc"
#undef WG_SDL3_FUNCTION
#endif
#include "../win32/WG_BACKEND.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static HMODULE library;
#else
#include <dlfcn.h>
static void *library;
#endif
#define WG_SDL3_FUNCTION(result,name,args) result (SDLCALL *wg_##name) args;
#include "WG_SDL3_FUNCTIONS.inc"
#undef WG_SDL3_FUNCTION

void WG_SDL3Unload(void)
{
#define WG_SDL3_FUNCTION(result,name,args) wg_##name = NULL;
#include "WG_SDL3_FUNCTIONS.inc"
#undef WG_SDL3_FUNCTION
    if (library) {
#ifdef _WIN32
        FreeLibrary(library);
#else
        dlclose(library);
#endif
    }
    library = NULL;
}

int WG_SDL3Load(char *reason, size_t size)
{
#ifdef _WIN32
    wchar_t path[32768], *slash;
    DWORD length;
    UINT previous;
    FARPROC address;
#else
    void *address;
#endif
    if (library) return 1;
#ifdef _WIN32
    length = GetModuleFileNameW(NULL, path, 32768);
    slash = length && length < 32768 ? wcsrchr(path, L'\\') : NULL;
    if (!slash || slash - path > 32750) {
        snprintf(reason, size, "Cannot locate SDL3.dll beside wolf3d.exe");
        return 0;
    }
    wcscpy(slash + 1, L"SDL3.dll");
    previous = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    SetErrorMode(previous | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    library = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    SetErrorMode(previous);
#else
    /* Executable RUNPATH includes $ORIGIN for bundled distributions. */
    library = dlopen("libSDL3.so.0", RTLD_NOW | RTLD_LOCAL);
#endif
    if (!library) {
        snprintf(reason, size, "SDL3 runtime could not be loaded");
        return 0;
    }
#ifdef _WIN32
#define WG_SDL3_SYMBOL(name) GetProcAddress(library, #name)
#else
#define WG_SDL3_SYMBOL(name) dlsym(library, #name)
#endif
#define WG_SDL3_FUNCTION(result,name,args) \
    address = WG_SDL3_SYMBOL(name); \
    if (!address) { \
        snprintf(reason, size, "SDL3 is missing required export %s", #name); \
        WG_SDL3Unload(); return 0; \
    } \
    memcpy((void *)&wg_##name, &address, sizeof(wg_##name));
#include "WG_SDL3_FUNCTIONS.inc"
#undef WG_SDL3_FUNCTION
#undef WG_SDL3_SYMBOL
    return 1;
}
