#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif
static unsigned pumps;
static int presented;
static int wolf3d_PumpFrame(int present)
{
    ++pumps;
    presented |= present;
    return 0;
}
#include "../platforms/sdl3/WG_SDL_MODAL_FRAME.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed: %s\n", #x); return 1; } } while (0)
static int SDLCALL push_worker(void *userdata)
{
    return SDL_PushEvent((SDL_Event *)userdata) ? 0 : 1;
}
int main(int argc, char **argv)
{
    SDL_Event event;
    SDL_Thread *worker;
    int result;
    unsigned i;
    (void)argc; (void)argv;
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    CHECK(SDL_AddEventWatch(WG_SDLModalFrameWatch, NULL));
    SDL_zero(event);
    event.type = SDL_EVENT_WINDOW_EXPOSED;
    event.window.windowID = 42;
    event.window.data1 = 1;
    CHECK(SDL_PushEvent(&event));
    CHECK(pumps == 0); /* No initialized host window. */
    wg_sdl_modal_window_id = 42;
    event.window.windowID = 43;
    CHECK(SDL_PushEvent(&event));
    CHECK(pumps == 0);
    event.window.windowID = 42;
    event.window.data1 = 0;
    CHECK(SDL_PushEvent(&event));
    CHECK(pumps == 0); /* Ordinary expose does not advance simulation. */
    event.window.data1 = 1;
    event.type = SDL_EVENT_WINDOW_RESIZED;
    CHECK(SDL_PushEvent(&event));
    CHECK(pumps == 0);
    event.type = SDL_EVENT_WINDOW_EXPOSED;
    worker = SDL_CreateThread(push_worker, "expose-test", &event);
    CHECK(worker != NULL);
    SDL_WaitThread(worker, &result);
    CHECK(result == 0 && pumps == 0);
    for (i = 0; i < 50; ++i) CHECK(SDL_PushEvent(&event));
    CHECK(pumps == 50 && presented == 0);
#ifdef _WIN32
    /* Exercise SDL's actual native modal timer, not just synthetic exposes. */
    {
        SDL_Window *window;
        HWND hwnd;
        MSG message;
        Uint64 until;
        unsigned before = pumps;
        CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO));
        window = SDL_CreateWindow("modal frame test", 320, 240, SDL_WINDOW_HIDDEN);
        CHECK(window != NULL);
        wg_sdl_modal_window_id = SDL_GetWindowID(window);
        hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                           SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        CHECK(hwnd != NULL);
        SendMessage(hwnd, WM_ENTERSIZEMOVE, 0, 0);
        until = SDL_GetTicks() + 150;
        while (SDL_GetTicks() < until)
        {
            while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            SDL_Delay(1);
        }
        SendMessage(hwnd, WM_EXITSIZEMOVE, 0, 0);
        CHECK(pumps > before && presented == 0);
        SDL_DestroyWindow(window);
        wg_sdl_modal_window_id = 0;
    }
#endif
    i = pumps;
    SDL_RemoveEventWatch(WG_SDLModalFrameWatch, NULL);
    CHECK(SDL_PushEvent(&event));
    CHECK(pumps == i);
    SDL_Quit();
    puts("SDL modal frames: live events, window/thread filtering, no presentation and removal passed.");
    return 0;
}
