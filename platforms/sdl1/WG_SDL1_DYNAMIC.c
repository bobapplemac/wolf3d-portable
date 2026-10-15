/* Explicit loading keeps SDL out of wolf3d.exe's required imports. */
#include <windows.h>
#include <stdio.h>
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
    wchar_t path[32768],*slash;DWORD n,failure;UINT previous;FARPROC address;
    const SDL_version *version;
    if(sdl_library)return 1;
    n=GetModuleFileNameW(NULL,path,32768);
    slash=n && n<32768?wcsrchr(path,L'\\'):NULL;
    if(!slash || (size_t)(slash-path)+9>=32768) {
        snprintf(reason,size,"Cannot locate SDL.dll beside wolf3d.exe");return 0;
    }
    wcscpy(slash+1,L"SDL.dll");
    previous=SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    SetErrorMode(previous|SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    sdl_library=LoadLibraryExW(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
    failure=GetLastError();SetErrorMode(previous);
    if(!sdl_library) {
        snprintf(reason,size,"SDL.dll could not be loaded (Windows error %lu)",(unsigned long)failure);return 0;
    }
#define WG_SDL1_FUNCTION(result,name,args) \
    address=GetProcAddress(sdl_library,#name); \
    if(!address) {snprintf(reason,size,"SDL.dll is missing required export %s",#name);WG_SDL1Unload();return 0;} \
    memcpy(&wg_##name,&address,sizeof(wg_##name));
#include "WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
    version=wg_SDL_Linked_Version();
    if(!version || version->major!=1 || version->minor!=2 || version->patch<10) {
        snprintf(reason,size,"SDL.dll must provide SDL 1.2.10 or newer in the 1.2 series");WG_SDL1Unload();return 0;
    }
    return 1;
}
