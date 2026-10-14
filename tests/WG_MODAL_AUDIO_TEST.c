#include <assert.h>
#include <stdio.h>
#include <windows.h>
static unsigned starts, stops, pumps;
static int fail;
#define WOLF3D_RESULT_OK 0
static int wolf3d_PumpAudio(void) { ++pumps; return fail; }
static UINT_PTR WINAPI fake_set(HWND window, UINT_PTR id, UINT ms, TIMERPROC proc)
{ (void)window; (void)ms; (void)proc; ++starts; return id; }
static BOOL WINAPI fake_kill(HWND window, UINT_PTR id)
{ (void)window; (void)id; ++stops; return TRUE; }
#define SetTimer fake_set
#define KillTimer fake_kill
#include "../platforms/win32/WG_MODAL_AUDIO.h"
int main(void)
{
    WG_ModalAudioTick(WG_MODAL_AUDIO_TIMER); assert(!pumps);
    WG_ModalAudioEnter(NULL); WG_ModalAudioEnter(NULL); assert(starts == 1);
    WG_ModalAudioTick(1); assert(!pumps);
    assert(WG_ModalAudioTick(WG_MODAL_AUDIO_TIMER) && pumps == 1);
    WG_ModalAudioLeave(NULL); assert(!stops);
    assert(WG_ModalAudioTick(WG_MODAL_AUDIO_TIMER) && pumps == 2);
    fail = 1; assert(!WG_ModalAudioTick(WG_MODAL_AUDIO_TIMER));
    WG_ModalAudioLeave(NULL); assert(stops == 1);
    WG_ModalAudioTick(WG_MODAL_AUDIO_TIMER); assert(pumps == 3);
    WG_ModalAudioLeave(NULL); assert(stops == 1);
    puts("Modal audio: drag/menu nesting, pumping, errors and stale timers passed.");
    return 0;
}
