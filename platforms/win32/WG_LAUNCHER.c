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
static int launch(int argc,char **argv,int *no_pause)
{
    wg_launcher_arguments_t options;char error[2048];const char *choice="auto";
    char **effective;int i,count=0,result=1,automatic;
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
        } else if(strcmp(options.argv[i],"--no-config"))effective[count++]=options.argv[i];
    }
    /* Config has already been merged. Backends must not read it a second time. */
    effective[count++]="--no-config";
    *no_pause=WG_NoPause(count,effective);
    WG_SetLauncherConfigPath(options.config_path);
    automatic=!strcmp(choice,"auto");
#if defined(WG_HAVE_GDI) && defined(WG_HAVE_SDL1)
    WG_SetVideoInfo("gdi sdl1",choice,NULL);
#elif defined(WG_HAVE_GDI)
    WG_SetVideoInfo("gdi",choice,NULL);
#else
    WG_SetVideoInfo("sdl1",choice,NULL);
#endif
#ifdef WG_HAVE_SDL1
    if(automatic || !strcmp(choice,"sdl1")) {
        char reason[256];
        if(WG_SDL1Load(reason,sizeof(reason))) {
            WG_SetVideoInfo(NULL,"sdl1",NULL);
            result=WG_SDL1Run(count,effective);
            WG_SDL1Unload();
            if(result!=WG_BACKEND_UNAVAILABLE)goto done;
            if(!automatic) {WG_PrintLauncherError("SDL1 video initialization failed.\n");result=1;goto done;}
            if(WG_CommandLineDiagnosticsRequested(count,effective))printf("SDL1 video initialization failed; trying GDI.\n");
        } else {
            if(WG_CommandLineDiagnosticsRequested(count,effective))printf("SDL1 unavailable: %s\n",reason);
            if(!automatic) {snprintf(error,sizeof(error),"%s\nKeep a compatible SDL.dll beside wolf3d.exe.\n",reason);WG_PrintLauncherError(error);goto done;}
        }
    }
#else
    if(!strcmp(choice,"sdl1")) {WG_PrintLauncherError("SDL1 is not compiled into this package.\n");goto done;}
#endif
#ifdef WG_HAVE_GDI
    if(automatic || !strcmp(choice,"gdi")) {
        WG_SetVideoInfo(NULL,"gdi",NULL);result=WG_GDIRun(count,effective);goto done;
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
