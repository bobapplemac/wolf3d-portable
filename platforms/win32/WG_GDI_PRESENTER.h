#ifndef WG_GDI_PRESENTER_H
#define WG_GDI_PRESENTER_H
/* Single pending frame: no GDI call or scaling runs under the mailbox lock. */
typedef struct wg_present_frame {
    uint8_t pixels[WOLF3D_SCREEN_WIDTH*WOLF3D_SCREEN_HEIGHT], palette[768];
    RECT client, view;
} wg_present_frame_t;
static wg_present_frame_t wg_present_pending, wg_present_work;
static CRITICAL_SECTION wg_present_lock;
static HANDLE wg_present_thread, wg_present_wake;
static HWND wg_present_window;
static int wg_present_stop, wg_present_ready;
static DWORD WINAPI WG_PresentWorker(LPVOID unused)
{
    struct {BITMAPINFOHEADER header;RGBQUAD colors[256];} info;
    (void)unused;
    memset(&info,0,sizeof(info));info.header.biSize=sizeof(info.header);
    info.header.biWidth=WOLF3D_SCREEN_WIDTH;info.header.biHeight=-WOLF3D_SCREEN_HEIGHT;
    info.header.biPlanes=1;info.header.biBitCount=8;info.header.biCompression=BI_RGB;
    for(;;) {
        HDC dc;int width,height,vw,vh,i;
        RECT view;
        WaitForSingleObject(wg_present_wake,INFINITE);
        EnterCriticalSection(&wg_present_lock);
        if(wg_present_stop) {LeaveCriticalSection(&wg_present_lock);break;}
        if(!wg_present_ready) {LeaveCriticalSection(&wg_present_lock);continue;}
        memcpy(&wg_present_work,&wg_present_pending,sizeof(wg_present_work));wg_present_ready=0;
        LeaveCriticalSection(&wg_present_lock);
        width=wg_present_work.client.right;height=wg_present_work.client.bottom;view=wg_present_work.view;
        vw=view.right-view.left;vh=view.bottom-view.top;
        if(width<=0 || height<=0 || vw<=0 || vh<=0)continue;
        for(i=0;i<256;++i) {
            info.colors[i].rgbRed=wg_present_work.palette[i*3];
            info.colors[i].rgbGreen=wg_present_work.palette[i*3+1];
            info.colors[i].rgbBlue=wg_present_work.palette[i*3+2];
        }
        dc=GetDC(wg_present_window);if(!dc)continue;
        SetStretchBltMode(dc,COLORONCOLOR);
        if(view.top)PatBlt(dc,0,0,width,view.top,BLACKNESS);
        if(view.bottom<height)PatBlt(dc,0,view.bottom,width,height-view.bottom,BLACKNESS);
        if(view.left)PatBlt(dc,0,view.top,view.left,vh,BLACKNESS);
        if(view.right<width)PatBlt(dc,view.right,view.top,width-view.right,vh,BLACKNESS);
        (void)StretchDIBits(dc,view.left,view.top,vw,vh,0,0,WOLF3D_SCREEN_WIDTH,WOLF3D_SCREEN_HEIGHT,
            wg_present_work.pixels,(BITMAPINFO *)&info,DIB_RGB_COLORS,SRCCOPY);
        GdiFlush();
        ReleaseDC(wg_present_window,dc);
    }
    return 0;
}
static int WG_PresenterStart(HWND window)
{
    wg_present_window=window;wg_present_stop=wg_present_ready=0;
    InitializeCriticalSection(&wg_present_lock);
    wg_present_wake=CreateEvent(NULL,FALSE,FALSE,NULL);
    if(!wg_present_wake) {DeleteCriticalSection(&wg_present_lock);return 0;}
    wg_present_thread=CreateThread(NULL,0,WG_PresentWorker,NULL,0,NULL);
    if(!wg_present_thread) {CloseHandle(wg_present_wake);DeleteCriticalSection(&wg_present_lock);return 0;}
    SetThreadPriority(wg_present_thread,THREAD_PRIORITY_BELOW_NORMAL);
    return 1;
}
static void WG_PresenterStop(void)
{
    if(!wg_present_thread)return;
    EnterCriticalSection(&wg_present_lock);wg_present_stop=1;
    LeaveCriticalSection(&wg_present_lock);SetEvent(wg_present_wake);
    /* Service synchronous window messages while joining a GDI caller. */
    while(MsgWaitForMultipleObjects(1,&wg_present_thread,FALSE,INFINITE,QS_SENDMESSAGE)!=WAIT_OBJECT_0) {
        MSG msg;PeekMessage(&msg,NULL,0,0,PM_NOREMOVE);
    }
    CloseHandle(wg_present_thread);CloseHandle(wg_present_wake);wg_present_thread=NULL;
    DeleteCriticalSection(&wg_present_lock);

}
static void WG_PresenterSubmit(const uint8_t *pixels,const uint8_t *palette,RECT client,RECT view)
{
    if(!wg_present_thread)return;
    EnterCriticalSection(&wg_present_lock);
    memcpy(wg_present_pending.pixels,pixels,sizeof(wg_present_pending.pixels));
    memcpy(wg_present_pending.palette,palette,sizeof(wg_present_pending.palette));
    wg_present_pending.client=client;wg_present_pending.view=view;wg_present_ready=1;
    LeaveCriticalSection(&wg_present_lock);SetEvent(wg_present_wake);
}
#endif
