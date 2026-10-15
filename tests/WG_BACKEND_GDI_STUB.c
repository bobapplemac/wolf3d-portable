#include <stdio.h>
#include <stdlib.h>
#include "../platforms/win32/WG_BACKEND.h"
int WG_GDIRun(int argc,char **argv)
{ (void)argc;(void)argv;puts("GDI test backend executed");return 0; }
int WG_TestSDL1Run(int argc,char **argv)
{
 const char *result=getenv("WG_TEST_BACKEND_RESULT");
 if(result) {puts("SDL test backend executed");return atoi(result);}
 return WG_SDL1Run(argc,argv);
}
