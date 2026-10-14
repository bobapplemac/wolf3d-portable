#ifndef WG_MODAL_FRAME_H
#define WG_MODAL_FRAME_H
/* Keep simulation and the memory framebuffer live in Windows' nested modal
   loop. Skip expensive GDI presentation until the drag/menu ends. */
#define WG_MODAL_FRAME_TIMER 0x5747U
static unsigned wg_modal_frame_depth;
static void WG_ModalFrameEnter(HWND window)
{
    if (!wg_modal_frame_depth++) SetTimer(window, WG_MODAL_FRAME_TIMER, 10, NULL);
}
static void WG_ModalFrameLeave(HWND window)
{
    if (wg_modal_frame_depth && !--wg_modal_frame_depth)
    {
        KillTimer(window, WG_MODAL_FRAME_TIMER);
        (void)wolf3d_PumpFrame(1);
    }
}
static int WG_ModalFrameTick(WPARAM timer)
{
    if (timer != WG_MODAL_FRAME_TIMER || !wg_modal_frame_depth) return 1;
    return wolf3d_PumpFrame(0) == WOLF3D_RESULT_OK;
}
#endif
