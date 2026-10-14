#ifndef WG_MODAL_AUDIO_H
#define WG_MODAL_AUDIO_H
/* Called only by the window/engine thread, including Windows' nested modal
   message loops. Never render engine audio from the port-playback worker. */
#define WG_MODAL_AUDIO_TIMER 0x5747U
static unsigned wg_modal_audio_depth;
static void WG_ModalAudioEnter(HWND window)
{
    if (!wg_modal_audio_depth++) SetTimer(window, WG_MODAL_AUDIO_TIMER, 10, NULL);
}
static void WG_ModalAudioLeave(HWND window)
{
    if (wg_modal_audio_depth && !--wg_modal_audio_depth)
        KillTimer(window, WG_MODAL_AUDIO_TIMER);
}
static int WG_ModalAudioTick(WPARAM timer)
{
    if (timer != WG_MODAL_AUDIO_TIMER || !wg_modal_audio_depth) return 1;
    return wolf3d_PumpAudio() == WOLF3D_RESULT_OK;
}
#endif
