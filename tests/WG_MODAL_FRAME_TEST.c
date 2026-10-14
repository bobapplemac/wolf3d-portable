#include <assert.h>
#include <stdio.h>
#include <windows.h>
static unsigned starts, stops, pumps, presents;
static int fail;
#define WOLF3D_RESULT_OK 0
static int wolf3d_PumpFrame(int present) { if (present) ++presents; else ++pumps; return fail; }
static UINT_PTR WINAPI fake_set(HWND window, UINT_PTR id, UINT ms, TIMERPROC proc)
{ (void)window; (void)ms; (void)proc; ++starts; return id; }
static BOOL WINAPI fake_kill(HWND window, UINT_PTR id)
{ (void)window; (void)id; ++stops; return TRUE; }
#define SetTimer fake_set
#define KillTimer fake_kill
#include "../platforms/win32/WG_MODAL_FRAME.h"
int main(void)
{
    WG_ModalFrameTick(WG_MODAL_FRAME_TIMER); assert(!pumps);
    WG_ModalFrameEnter(NULL); WG_ModalFrameEnter(NULL); assert(starts == 1);
    WG_ModalFrameTick(1); assert(!pumps);
    assert(WG_ModalFrameTick(WG_MODAL_FRAME_TIMER) && pumps == 1);
    WG_ModalFrameLeave(NULL); assert(!stops);
    assert(WG_ModalFrameTick(WG_MODAL_FRAME_TIMER) && pumps == 2);
    fail = 1; assert(!WG_ModalFrameTick(WG_MODAL_FRAME_TIMER));
    WG_ModalFrameLeave(NULL); assert(stops == 1 && presents == 1);
    WG_ModalFrameTick(WG_MODAL_FRAME_TIMER); assert(pumps == 3);
    WG_ModalFrameLeave(NULL); assert(stops == 1);
    puts("Modal frame: drag/menu nesting, pumping, errors and stale timers passed.");
    return 0;
}
