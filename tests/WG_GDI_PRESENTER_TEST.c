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
static int WINAPI slow_blit(HDC dc,int x,int y,int w,int h,int sx,int sy,int sw,int sh,
    const void *bits,const BITMAPINFO *info,UINT usage,DWORD rop)
{
    LONG call=InterlockedIncrement(&calls);
    (void)dc;(void)x;(void)y;(void)sx;(void)sy;(void)usage;(void)rop;
    assert(sw==320 && sh==200 && info->bmiHeader.biBitCount==8 && ((const uint8_t *)bits)[0]==1);
    if(call==1) {
        assert(w==320 && h==240 && info->bmiColors[1].rgbRed==255);
        SetEvent(entered);assert(WaitForSingleObject(release_blit,5000)==WAIT_OBJECT_0);
        assert(info->bmiColors[1].rgbRed==255 && ((const uint8_t *)bits)[0]==1);
    } else {
        assert(w==640 && h==480 && info->bmiColors[1].rgbBlue==255);
        SetEvent(second);SendMessage(test_window,WM_NULL,0,0);
    }
    return 200;
}
#define StretchDIBits slow_blit
/* Enforce the Win9x API contract even when tests run on NT. */
static int fail_thread;
static HANDLE WINAPI win9x_create_thread(LPSECURITY_ATTRIBUTES security, SIZE_T stack,
    LPTHREAD_START_ROUTINE start, LPVOID argument, DWORD flags, LPDWORD id)
{
    if(!id || fail_thread) {SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
    return CreateThread(security,stack,start,argument,flags,id);
}
static BOOL WINAPI clobber_close_handle(HANDLE handle)
{
    BOOL result=CloseHandle(handle);
    SetLastError(ERROR_SUCCESS);
    return result;
}
#define CloseHandle clobber_close_handle
#define CreateThread win9x_create_thread
#include "../platforms/win32/WG_GDI_PRESENTER.h"
#undef CreateThread
#undef CloseHandle
#undef StretchDIBits
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
    fail_thread=1;assert(!WG_PresenterStart(window));assert(GetLastError()==ERROR_INVALID_PARAMETER);
    fail_thread=0;assert(WG_PresenterStart(window));
    memset(pixels,1,sizeof(pixels));memset(palette,0,sizeof(palette));palette[3]=255;
    WG_PresenterSubmit(pixels,palette,small,small);
    assert(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);
    palette[3]=0;palette[4]=255;WG_PresenterSubmit(pixels,palette,large,large);
    palette[4]=0;palette[5]=255;WG_PresenterSubmit(pixels,palette,large,large);
    assert(wg_present_ready);
    SetEvent(release_blit);
    assert(WaitForSingleObject(second,5000)==WAIT_OBJECT_0);
    WG_PresenterStop();assert(calls==2);
    WG_PresenterStop(); /* Idempotent. */
    fail_thread=1;assert(!WG_PresenterStart(window));assert(GetLastError()==ERROR_INVALID_PARAMETER);
    fail_thread=0;assert(WG_PresenterStart(window));WG_PresenterStop(); /* Idle restart/stop. */
    DestroyWindow(window);UnregisterClass(wc.lpszClassName,wc.hInstance);
    CloseHandle(entered);CloseHandle(release_blit);CloseHandle(second);
    puts("GDI presenter: nonblocking publication, replacement, immutable frame, resize and shutdown passed");
    return 0;
}
