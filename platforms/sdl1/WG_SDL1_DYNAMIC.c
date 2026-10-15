#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Explicit loading keeps SDL out of wolf3d.exe's required imports. */
#include <windows.h>
#include <stdio.h>
#include "../WG_PRINTF_COMPAT.h"
#include <string.h>
#include <wchar.h>
#define WG_SDL1_NO_ALIASES
#include "WG_SDL1_DYNAMIC.h"
#include "../win32/WG_BACKEND.h"
#define WG_SDL1_FUNCTION(result,name,args) result (SDLCALL *wg_##name) args;
#include "WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
static HMODULE sdl_library;
void WG_SDL1Unload(void)
{
    /* Call only after SDL audio and window workers have stopped. */
#define WG_SDL1_FUNCTION(result,name,args) wg_##name=NULL;
#include "WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
    if(sdl_library)FreeLibrary(sdl_library);
    sdl_library=NULL;
}
int WG_SDL1Load(char *reason,size_t size)
{
#ifdef WG_WIN9X
    char path[32768],*slash;
#else
    wchar_t path[32768],*slash;
#endif
    DWORD n,failure;UINT previous;FARPROC address;
    const SDL_version *version;
    if(sdl_library)return 1;
#ifdef WG_WIN9X
    n=GetModuleFileNameA(NULL,path,32768);
    slash=n && n<32768?strrchr(path,'\\'):NULL;
#else
    n=GetModuleFileNameW(NULL,path,32768);
    slash=n && n<32768?wcsrchr(path,L'\\'):NULL;
#endif
    if(!slash || (size_t)(slash-path)+9>=32768) {
        snprintf(reason,size,"Cannot locate SDL.dll beside wolf3d.exe");return 0;
    }
#ifdef WG_WIN9X
    strcpy(slash+1,"SDL.dll");
#else
    wcscpy(slash+1,L"SDL.dll");
#endif
    previous=SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    SetErrorMode(previous|SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#ifdef WG_WIN9X
    sdl_library=LoadLibraryExA(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    sdl_library=LoadLibraryExW(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
#endif
    failure=GetLastError();SetErrorMode(previous);
    if(!sdl_library) {
        snprintf(reason,size,"SDL.dll could not be loaded (Windows error %lu)",(unsigned long)failure);return 0;
    }
#define WG_SDL1_FUNCTION(result,name,args) \
    address=GetProcAddress(sdl_library,#name); \
    if(!address) {snprintf(reason,size,"SDL.dll is missing required export %s",#name);WG_SDL1Unload();return 0;} \
    memcpy((void *)&wg_##name,&address,sizeof(wg_##name));
#include "WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
    version=wg_SDL_Linked_Version();
    if(!version || version->major!=1 || version->minor!=2 || version->patch<10) {
        snprintf(reason,size,"SDL.dll must provide SDL 1.2.10 or newer in the 1.2 series");WG_SDL1Unload();return 0;
    }
    return 1;
}
