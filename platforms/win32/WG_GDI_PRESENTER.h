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
static int wg_present_stop, wg_present_ready, wg_present_trace;
static uint64_t wg_present_published, wg_present_replaced;
static uint64_t wg_present_draws, wg_present_total, wg_present_longest;
static uint64_t wg_present_blits, wg_present_blit_longest, wg_present_failures;
static unsigned wg_present_width,wg_present_height;
static DWORD WINAPI WG_PresentWorker(LPVOID unused)
{
    struct {BITMAPINFOHEADER header;RGBQUAD colors[256];} info;
    (void)unused;
    memset(&info,0,sizeof(info));info.header.biSize=sizeof(info.header);
    info.header.biWidth=WOLF3D_SCREEN_WIDTH;info.header.biHeight=-WOLF3D_SCREEN_HEIGHT;
    info.header.biPlanes=1;info.header.biBitCount=8;info.header.biCompression=BI_RGB;
    for(;;) {
        HDC dc;int width,height,vw,vh,i,result,timed;
        LARGE_INTEGER start,blit,end,done;
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
        timed=wg_present_trace && QueryPerformanceCounter(&start);
        for(i=0;i<256;++i) {
            info.colors[i].rgbRed=wg_present_work.palette[i*3];
            info.colors[i].rgbGreen=wg_present_work.palette[i*3+1];
            info.colors[i].rgbBlue=wg_present_work.palette[i*3+2];
        }
        dc=GetDC(wg_present_window);if(!dc) {++wg_present_failures;continue;}
        SetStretchBltMode(dc,COLORONCOLOR);
        if(timed)timed=QueryPerformanceCounter(&blit);
        if(view.top)PatBlt(dc,0,0,width,view.top,BLACKNESS);
        if(view.bottom<height)PatBlt(dc,0,view.bottom,width,height-view.bottom,BLACKNESS);
        if(view.left)PatBlt(dc,0,view.top,view.left,vh,BLACKNESS);
        if(view.right<width)PatBlt(dc,view.right,view.top,width-view.right,vh,BLACKNESS);
        result=StretchDIBits(dc,view.left,view.top,vw,vh,0,0,WOLF3D_SCREEN_WIDTH,WOLF3D_SCREEN_HEIGHT,
            wg_present_work.pixels,(BITMAPINFO *)&info,DIB_RGB_COLORS,SRCCOPY);
        if(result==0 || result==(int)GDI_ERROR)++wg_present_failures;
        GdiFlush();
        if(timed)timed=QueryPerformanceCounter(&end);
        ReleaseDC(wg_present_window,dc);
        if(timed && QueryPerformanceCounter(&done) && done.QuadPart>=start.QuadPart && end.QuadPart>=blit.QuadPart) {
            uint64_t total=(uint64_t)(done.QuadPart-start.QuadPart),draw=(uint64_t)(end.QuadPart-blit.QuadPart);
            ++wg_present_draws;wg_present_total+=total;wg_present_blits+=draw;
            if(total>wg_present_longest)wg_present_longest=total;
            if(draw>wg_present_blit_longest)wg_present_blit_longest=draw;
            wg_present_width=(unsigned)vw;wg_present_height=(unsigned)vh;
        }
    }
    return 0;
}
static int WG_PresenterStart(HWND window)
{
    const char *trace=getenv("WOLF3D_OPL_TIMING");
    wg_present_window=window;wg_present_stop=wg_present_ready=0;
    wg_present_trace=trace && strcmp(trace,"1")==0;
    wg_present_published=wg_present_replaced=wg_present_draws=0;
    wg_present_total=wg_present_longest=wg_present_blits=wg_present_blit_longest=0;
    wg_present_failures=0;wg_present_width=wg_present_height=0;
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
    if(wg_present_trace) {
        LARGE_INTEGER frequency;
        FILE *file;
        if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0)return;
        file=fopen("OPLTIME.TXT","a");
        if(file) {
            double hz=(double)frequency.QuadPart;
            fprintf(file,"Async GDI presentation: native 320x200 indexed upload + StretchDIBits COLORONCOLOR\n");
            fprintf(file,"Published=%.0f; replaced pending=%.0f; drawn=%.0f; failures=%.0f; last destination=%ux%u\n",
                (double)wg_present_published,(double)wg_present_replaced,(double)wg_present_draws,
                (double)wg_present_failures,wg_present_width,wg_present_height);
            fprintf(file,"Worker presentation: total=%.6f s; average=%.3f ms; longest=%.3f ms\n",
                (double)wg_present_total/hz,wg_present_draws ? (double)wg_present_total*1000/hz/wg_present_draws:0,
                (double)wg_present_longest*1000/hz);
            fprintf(file,"StretchDIBits + worker GdiFlush: total=%.6f s; longest=%.3f ms\n\n",
                (double)wg_present_blits/hz,(double)wg_present_blit_longest*1000/hz);
            fclose(file);
        }
    }
}
static void WG_PresenterSubmit(const uint8_t *pixels,const uint8_t *palette,RECT client,RECT view)
{
    if(!wg_present_thread)return;
    EnterCriticalSection(&wg_present_lock);
    ++wg_present_published;if(wg_present_ready)++wg_present_replaced;
    memcpy(wg_present_pending.pixels,pixels,sizeof(wg_present_pending.pixels));
    memcpy(wg_present_pending.palette,palette,sizeof(wg_present_pending.palette));
    wg_present_pending.client=client;wg_present_pending.view=view;wg_present_ready=1;
    LeaveCriticalSection(&wg_present_lock);SetEvent(wg_present_wake);
}
#endif
