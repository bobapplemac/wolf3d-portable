#ifndef WG_SDL_MODAL_FRAME_H
#define WG_SDL_MODAL_FRAME_H
/* SDL sends live-resize exposes inside native modal loops. Pump only memory
   frames: renderer calls here can be expensive or reenter the window system.
   PumpFrame latches failures for Run and guards recursion/out-of-run calls. */
static SDL_WindowID wg_sdl_modal_window_id;
static bool SDLCALL WG_SDLModalFrameWatch(void *userdata, SDL_Event *event)
{
    (void)userdata;
    /* Check the thread before reading host state: users can push events from
       worker threads, even though OS expose events arrive on the main thread. */
    if (SDL_IsMainThread() && wg_sdl_modal_window_id != 0
        && event->type == SDL_EVENT_WINDOW_EXPOSED
        && event->window.windowID == wg_sdl_modal_window_id
        && event->window.data1 == 1)
    {
        (void)wolf3d_PumpFrame(0);
    }
    return true;
}
#endif
