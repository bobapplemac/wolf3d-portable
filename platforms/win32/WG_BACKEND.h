#ifndef WG_BACKEND_H
#define WG_BACKEND_H
#include <stddef.h>
/* Private host selection boundary; no separate backend DLL or plugin ABI. */
#define WG_BACKEND_UNAVAILABLE (-2)
int WG_SDL1Load(char *reason,size_t size);
void WG_SDL1Unload(void);
int WG_SDL1Run(int argc,char **argv);
#endif
