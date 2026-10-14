#undef NDEBUG
#include <assert.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WOLF3D_SCREEN_WIDTH 320
#define WOLF3D_SCREEN_HEIGHT 200
static HANDLE entered,release_blit,second;
static LONG calls;
static HWND test_window;
static BOOL WINAPI slow_blit(HDC dc,int x,int y,int w,int h,HDC src,int sx,int sy,DWORD rop)
{
    LONG call=InterlockedIncrement(&calls);
    if(call==1) {
        assert(w==320 && h==240 && GetPixel(src,0,0)==RGB(255,0,0));
        SetEvent(entered);assert(WaitForSingleObject(release_blit,5000)==WAIT_OBJECT_0);
        assert(GetPixel(src,0,0)==RGB(255,0,0)); /* Producer cannot mutate in-flight DIB. */
    } else {
        assert(w==640 && h==480 && GetPixel(src,0,0)==RGB(0,0,255));
        SetEvent(second);
        SendMessage(test_window,WM_NULL,0,0); /* Stop must service cross-thread sends. */
    }
    /* Hidden windows may have no drawable DC: exercise the real memory blit. */
    (void)dc;
    return BitBlt(src,x,y,w,h,src,sx,sy,rop);
}
#define BitBlt slow_blit
#include "../platforms/win32/WG_GDI_PRESENTER.h"
#undef BitBlt
int main(void)
{
    WNDCLASS wc;HWND window;RECT small={0,0,320,240},large={0,0,640,480};
    uint8_t pixels[320*200],palette[768];
    ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=DefWindowProc;
    wc.hInstance=GetModuleHandle(NULL);wc.lpszClassName=TEXT("wg-present-test");
    assert(RegisterClass(&wc));
    window=CreateWindow(wc.lpszClassName,TEXT("hidden test"),WS_OVERLAPPEDWINDOW,
        0,0,640,480,NULL,NULL,wc.hInstance,NULL);assert(window);
    test_window=window;
    entered=CreateEvent(NULL,TRUE,FALSE,NULL);release_blit=CreateEvent(NULL,TRUE,FALSE,NULL);
    second=CreateEvent(NULL,TRUE,FALSE,NULL);assert(entered && release_blit && second);
    assert(WG_PresenterStart(window));
    memset(pixels,1,sizeof(pixels));memset(palette,0,sizeof(palette));palette[3]=255;
    WG_PresenterSubmit(pixels,palette,small,small);
    assert(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);
    palette[3]=0;palette[4]=255;WG_PresenterSubmit(pixels,palette,large,large);
    palette[4]=0;palette[5]=255;WG_PresenterSubmit(pixels,palette,large,large);
    assert(wg_present_published==3 && wg_present_replaced==1);
    SetEvent(release_blit);
    assert(WaitForSingleObject(second,5000)==WAIT_OBJECT_0);
    WG_PresenterStop();assert(calls==2 && !wg_present_failures);
    WG_PresenterStop(); /* Idempotent. */
    assert(WG_PresenterStart(window));WG_PresenterStop(); /* Idle restart/stop. */
    DestroyWindow(window);UnregisterClass(wc.lpszClassName,wc.hInstance);
    CloseHandle(entered);CloseHandle(release_blit);CloseHandle(second);
    puts("GDI presenter: nonblocking publication, replacement, immutable frame, resize and shutdown passed");
    return 0;
}
