#include <stdlib.h>
#include <stdio.h>
#ifndef TEST_ABI
#define TEST_ABI 1
#endif
__declspec(dllexport) int __cdecl WG_BackendVersion(void) {return TEST_ABI;}
__declspec(dllexport) int __cdecl WG_BackendRun(int argc,char **argv)
{
 const char *result=getenv("WG_TEST_BACKEND_RESULT");
 (void)argc;(void)argv;puts("SDL test backend executed");return result?atoi(result):0;
}
