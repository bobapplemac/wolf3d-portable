#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Win2k combined host. GDI is linked in; SDL1 and its dependencies are optional. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../WG_HELP.h"
#include "WG_BACKEND.h"
#include "WG_WIN32_CONSOLE.h"
#include "../WG_BACKENDS.c"
#ifdef WG_WIN9X
int main(int argc, char **argv)
{
    int owned = WG_OwnConsole(), no_pause = WG_NoPause(argc, argv);
    int result = WG_LaunchBackends(argc, argv, &no_pause);
    WG_PauseConsole(owned, no_pause);
    return result;
}
#else
int wmain(int argc,wchar_t **wide)
{
    int i,result=1,owned=WG_OwnConsole(),no_pause=0;char **argv;
    wchar_t module_path[32768];DWORD path_length=GetModuleFileNameW(NULL,module_path,32768);
    argv=(char **)calloc((size_t)argc+1,sizeof(*argv));if(!argv)return 1;
    for(i=0;i<argc;++i) {
        const wchar_t *argument=i==0 && path_length && path_length<32768?module_path:wide[i];
        int n=WideCharToMultiByte(CP_UTF8,0,argument,-1,NULL,0,NULL,NULL);
        if(!n)goto done;
        argv[i]=(char *)malloc((size_t)n);
        if(!argv[i])goto done;
        WideCharToMultiByte(CP_UTF8,0,argument,-1,argv[i],n,NULL,NULL);
    }
    no_pause=WG_NoPause(argc,argv);
    result=WG_LaunchBackends(argc,argv,&no_pause);
done:
    for(i=0;i<argc;++i)free(argv[i]);
    free(argv);
    WG_PauseConsole(owned,no_pause);return result;
}

#endif
