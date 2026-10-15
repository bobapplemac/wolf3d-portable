#undef NDEBUG
#include <assert.h>
#include <string.h>
#define WG_HAVE_SDL3 1
#define WG_HAVE_GDI 1
#include "../platforms/WG_BACKENDS.c"
static int available, sdl_result, sdl_runs, gdi_runs, unloads;
int WG_SDL3Load(char *reason, size_t size)
{
    (void)size;
    strcpy(reason, "injected unavailable SDL");
    return available;
}
void WG_SDL3Unload(void) { ++unloads; }
int WG_SDL3Run(int argc, char **argv)
{ (void)argc; (void)argv; ++sdl_runs; return sdl_result; }
int WG_GDIRun(int argc, char **argv)
{ (void)argc; (void)argv; ++gdi_runs; return 0; }
static int launch_choice(const char *choice)
{
    char *args[6];
    int no_pause = 0;
    args[0] = "backend-test"; args[1] = "--no-config";
    args[2] = "--backend"; args[3] = (char *)choice;
    args[4] = "--no-pause"; args[5] = NULL;
    sdl_runs = gdi_runs = unloads = 0;
    return WG_LaunchBackends(5, args, &no_pause);
}
int main(void)
{
    available = 0;
    assert(launch_choice("auto") == 0 && gdi_runs == 1 && !sdl_runs && !unloads);
    assert(launch_choice("sdl3") == 1 && !gdi_runs);
    available = 1; sdl_result = WG_BACKEND_UNAVAILABLE;
    assert(launch_choice("auto") == 0 && sdl_runs == 1 && gdi_runs == 1 && unloads == 1);
    assert(launch_choice("sdl3") == 1 && sdl_runs == 1 && !gdi_runs && unloads == 1);
    /* General game/OPL/data errors are not permission to switch hosts. */
    sdl_result = 1;
    assert(launch_choice("auto") == 1 && sdl_runs == 1 && !gdi_runs && unloads == 1);
    sdl_result = 0;
    assert(launch_choice("auto") == 0 && sdl_runs == 1 && !gdi_runs && unloads == 1);
    assert(launch_choice("gdi") == 0 && gdi_runs == 1 && !sdl_runs);
    assert(launch_choice("sdl1") == 1 && !gdi_runs && !sdl_runs);
    return 0;
}
