#undef NDEBUG
#include <assert.h>
#include <windows.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#define WG_SDL1_NO_ALIASES
#include "../platforms/sdl1/WG_SDL1_DYNAMIC.h"
#undef main
/* Compile-check every pointer signature against the installed SDL headers. */
#define WG_SDL1_FUNCTION(result,name,args) typedef char check_##name[__builtin_types_compatible_p(__typeof__(wg_##name),__typeof__(&name))?1:-1];
#include "../platforms/sdl1/WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
static int loads,frees,load_failed;
static UINT error_mode=0x4000;
static const char *missing;
static SDL_version version={1,2,15};
static const SDL_version *SDLCALL linked_version(void) {return &version;}
static void unused_function(void) {}
static DWORD WINAPI mock_path(HMODULE module,LPWSTR path,DWORD size)
{ (void)module;(void)size;wcscpy(path,L"C:\\test\\wolf3d.exe");return (DWORD)wcslen(path); }
static UINT WINAPI mock_error_mode(UINT mode)
{ UINT previous=error_mode;error_mode=mode;return previous; }
static HMODULE WINAPI mock_load(LPCWSTR path,HANDLE file,DWORD flags)
{
 (void)file;assert(!wcscmp(path,L"C:\\test\\SDL.dll"));assert(flags==LOAD_WITH_ALTERED_SEARCH_PATH);
 assert(error_mode & SEM_FAILCRITICALERRORS);++loads;return load_failed?NULL:(HMODULE)1;
}
static BOOL WINAPI mock_free(HMODULE module) {assert(module==(HMODULE)1);++frees;return TRUE;}
static FARPROC WINAPI mock_proc(HMODULE module,LPCSTR name)
{
 FARPROC proc;assert(module==(HMODULE)1);
 if(missing && !strcmp(missing,name))return NULL;
 if(!strcmp(name,"SDL_Linked_Version")) {
  const SDL_version *(SDLCALL *fn)(void)=linked_version;memcpy(&proc,&fn,sizeof(proc));
 } else {void (*fn)(void)=unused_function;memcpy(&proc,&fn,sizeof(proc));}
 return proc;
}
#define GetModuleFileNameW mock_path
#define SetErrorMode mock_error_mode
#define LoadLibraryExW mock_load
#define FreeLibrary mock_free
#define GetProcAddress mock_proc
#include "../platforms/sdl1/WG_SDL1_DYNAMIC.c"
static void all_clear(void)
{
#define WG_SDL1_FUNCTION(result,name,args) assert(!wg_##name);
#include "../platforms/sdl1/WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
 assert(!sdl_library);assert(error_mode==0x4000);
}
int main(void)
{
 char reason[256];int before;
 assert(WG_SDL1Load(reason,sizeof(reason)));before=loads;
 assert(WG_SDL1Load(reason,sizeof(reason)) && loads==before);
 WG_SDL1Unload();all_clear();before=frees;WG_SDL1Unload();assert(frees==before);
#define WG_SDL1_FUNCTION(result,name,args) missing=#name;assert(!WG_SDL1Load(reason,sizeof(reason)));assert(strstr(reason,#name));all_clear();
#include "../platforms/sdl1/WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
 missing=NULL;version.major=2;assert(!WG_SDL1Load(reason,sizeof(reason)));all_clear();
 version.major=1;version.patch=9;assert(!WG_SDL1Load(reason,sizeof(reason)));all_clear();
 version.patch=15;load_failed=1;before=frees;
 assert(!WG_SDL1Load(reason,sizeof(reason)));all_clear();assert(frees==before);
 load_failed=0;assert(WG_SDL1Load(reason,sizeof(reason)));WG_SDL1Unload();all_clear();
 assert(loads==frees+1);puts("SDL1 dynamic exports: signatures, each missing export, rollback, version, reload and error mode passed");return 0;
}
