#ifndef WG_BACKEND_H
#define WG_BACKEND_H
/* Private portable-host ABI, independent of the engine platform API. */
#define WG_BACKEND_ABI 1
#define WG_BACKEND_UNAVAILABLE (-2)
typedef int (__cdecl *wg_backend_version_t)(void);
typedef int (__cdecl *wg_backend_run_t)(int argc,char **argv);
#endif
