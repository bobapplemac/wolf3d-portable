/* Win2k combined host. GDI is linked in; SDL1 and its dependencies are optional. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../WG_HELP.h"
#include "WG_BACKEND.h"
#include "WG_WIN32_CONSOLE.h"
#ifdef WG_HAVE_GDI
int WG_GDIRun(int argc,char **argv);
#endif
#ifdef WG_HAVE_SDL1
static HMODULE sdl_library,backend_library;
static wg_backend_run_t backend_run;
static int adjacent_path(const wchar_t *file,wchar_t *path)
{
    DWORD n=GetModuleFileNameW(NULL,path,32768);wchar_t *slash;
    if(!n || n>=32768)return 0;
    slash=wcsrchr(path,L'\\');if(!slash)return 0;
    if((size_t)(slash-path)+1+wcslen(file)>=32768)return 0;
    wcscpy(slash+1,file);return 1;
}
static void close_sdl(void)
{
    backend_run=NULL;
    if(backend_library)FreeLibrary(backend_library);
    if(sdl_library)FreeLibrary(sdl_library);
    backend_library=sdl_library=NULL;
}
static int open_sdl(char *reason,size_t size)
{
    wchar_t path[32768];UINT previous;DWORD failure=0;FARPROC address;
    wg_backend_version_t version;const char *component="SDL.dll";
    previous=SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    if(!adjacent_path(L"SDL.dll",path))failure=ERROR_BAD_PATHNAME;
    else sdl_library=LoadLibraryExW(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!failure && !sdl_library)failure=GetLastError();
    if(!failure) {
        component="wolf3d-sdl1.dll";
        if(!adjacent_path(L"wolf3d-sdl1.dll",path))failure=ERROR_BAD_PATHNAME;
        else backend_library=LoadLibraryExW(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!failure && !backend_library)failure=GetLastError();
    }
    SetErrorMode(previous);
    if(failure) {
        snprintf(reason,size,"%s could not be loaded (Windows error %lu)",component,(unsigned long)failure);
        close_sdl();return 0;
    }
    address=GetProcAddress(backend_library,"WG_BackendVersion");memcpy(&version,&address,sizeof(version));
    address=GetProcAddress(backend_library,"WG_BackendRun");memcpy(&backend_run,&address,sizeof(backend_run));
    if(!version || !backend_run || version()!=WG_BACKEND_ABI) {
        snprintf(reason,size,"wolf3d-sdl1.dll has an incompatible backend ABI");close_sdl();return 0;
    }
    return 1;
}
#endif
static int launch(int argc,char **argv,int *no_pause)
{
    wg_launcher_arguments_t options;char error[2048];const char *choice="auto";
    char **effective;int i,count=0,result=1,automatic,diag,help;
    if(!WG_LoadLauncherArguments(argc,argv,&options,error,sizeof(error))) {WG_PrintLauncherError(error);return 1;}
    effective=(char **)calloc((size_t)options.argc+2,sizeof(*effective));
    if(!effective) {WG_FreeLauncherArguments(&options);return 1;}
    effective[count++]=options.argv[0];
    for(i=1;i<options.argc;++i) {
        if(!strcmp(options.argv[i],"--backend")) {
            if(++i==options.argc || (strcmp(options.argv[i],"auto") && strcmp(options.argv[i],"gdi") && strcmp(options.argv[i],"sdl1"))) {
                WG_PrintLauncherError("--backend requires auto, gdi or sdl1.\n");goto done;
            }
            choice=options.argv[i];
        } else effective[count++]=options.argv[i];
    }
    /* Config has already been merged. Backends must not read it a second time. */
    effective[count++]="--no-config";
    *no_pause=WG_NoPause(count,effective);
    automatic=!strcmp(choice,"auto");
    diag=WG_CommandLineDiagnosticsRequested(count,effective);
    help=WG_CommandLineHelpRequested(count,effective);
    if(help || diag) {
        printf("Compiled backends:");
#ifdef WG_HAVE_SDL1
        printf(" sdl1");
#endif
#ifdef WG_HAVE_GDI
        printf(" gdi");
#endif
        printf("\nAuto preference: SDL1, then GDI (compiled backends only)\n");
        printf("--backend auto|gdi|sdl1 selects the complete video/input/PCM host.\n");
        if(options.config_path)printf("Launcher config: %s\n",options.config_path);
    }
    if(diag) {
#ifdef WG_HAVE_GDI
        printf("GDI availability: built in (no SDL dependency)\n");
#endif
#ifdef WG_HAVE_SDL1
        char reason[256];
        if(open_sdl(reason,sizeof(reason))) {printf("SDL1 availability: module and SDL.dll loaded; backend ABI verified\n");close_sdl();}
        else printf("SDL1 availability: %s\n",reason);
#endif
    }
#ifdef WG_HAVE_SDL1
    if(automatic || !strcmp(choice,"sdl1")) {
        char reason[256];
        if(open_sdl(reason,sizeof(reason))) {
            printf("Backend: sdl1\n");
            result=backend_run(count,effective);
            close_sdl();
            if(result!=WG_BACKEND_UNAVAILABLE)goto done;
            if(!automatic) {WG_PrintLauncherError("SDL1 video initialization failed.\n");result=1;goto done;}
            if(diag)printf("SDL1 video initialization failed; trying GDI.\n");
        } else {
            if(diag)printf("SDL1 unavailable: %s\n",reason);
            if(!automatic) {snprintf(error,sizeof(error),"%s\nKeep matching SDL.dll and wolf3d-sdl1.dll beside wolf3d.exe.\n",reason);WG_PrintLauncherError(error);goto done;}
        }
    }
#else
    if(!strcmp(choice,"sdl1")) {WG_PrintLauncherError("SDL1 is not compiled into this package.\n");goto done;}
#endif
#ifdef WG_HAVE_GDI
    if(automatic || !strcmp(choice,"gdi")) {
        printf("Backend: gdi\n");result=WG_GDIRun(count,effective);goto done;
    }
#else
    if(!strcmp(choice,"gdi")) {WG_PrintLauncherError("GDI is not compiled into this package.\n");goto done;}
#endif
    WG_PrintLauncherError("No usable compiled backend is available.\n");result=1;
done:
    free(effective);WG_FreeLauncherArguments(&options);return result;
}
int wmain(int argc,wchar_t **wide)
{
    int i,result=1,owned=WG_OwnConsole(),no_pause=0;char **argv;
    wchar_t module_path[32768];DWORD path_length=GetModuleFileNameW(NULL,module_path,32768);
    argv=(char **)calloc((size_t)argc+1,sizeof(*argv));if(!argv)return 1;
    for(i=0;i<argc;++i) {
        const wchar_t *argument=i==0 && path_length && path_length<32768?module_path:wide[i];
        int n=WideCharToMultiByte(CP_UTF8,0,argument,-1,NULL,0,NULL,NULL);
        if(!n || !(argv[i]=(char *)malloc((size_t)n)))goto done;
        WideCharToMultiByte(CP_UTF8,0,argument,-1,argv[i],n,NULL,NULL);
    }
    no_pause=WG_NoPause(argc,argv);
    result=launch(argc,argv,&no_pause);
done:
    for(i=0;i<argc;++i)free(argv[i]);
    free(argv);
    WG_PauseConsole(owned,no_pause);return result;
}
