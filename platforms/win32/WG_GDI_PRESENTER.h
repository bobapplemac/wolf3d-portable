#ifndef WG_GDI_PRESENTER_H
#define WG_GDI_PRESENTER_H
#include "WG_NEAREST_SCALE.h"
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
static unsigned wg_present_width, wg_present_height, wg_present_bpp, wg_desktop_bpp;
static int wg_present_ddb, wg_present_force32;
static DWORD WINAPI WG_PresentWorker(LPVOID unused)
{
    HDC memory=NULL,upload=NULL;
    HBITMAP compatible=NULL;
    HGDIOBJ upload_original=NULL;
    struct { BITMAPINFOHEADER header; DWORD masks[3]; } info;
    HBITMAP bitmap=NULL;
    HGDIOBJ original=NULL;
    void *bits=NULL;
    unsigned stride=0,bpp=32;
    int rgb565=1;
    unsigned *xmap=NULL;
    int bw=0,bh=0,mw=0;
    RECT last_view;
    ZeroMemory(&last_view,sizeof(last_view));
    (void)unused;
    for(;;) {
        HDC dc;
        int width,height,vw,vh;
        LARGE_INTEGER start,blit,end,done;
        int timed;
        WaitForSingleObject(wg_present_wake,INFINITE);
        EnterCriticalSection(&wg_present_lock);
        if(wg_present_stop) { LeaveCriticalSection(&wg_present_lock);break; }
        if(!wg_present_ready) { LeaveCriticalSection(&wg_present_lock);continue; }
        memcpy(&wg_present_work,&wg_present_pending,sizeof(wg_present_work));
        wg_present_ready=0;
        LeaveCriticalSection(&wg_present_lock);
        width=wg_present_work.client.right; height=wg_present_work.client.bottom;
        vw=wg_present_work.view.right-wg_present_work.view.left;
        vh=wg_present_work.view.bottom-wg_present_work.view.top;
        if(width<=0 || height<=0 || vw<=0 || vh<=0)continue;
        timed=wg_present_trace && QueryPerformanceCounter(&start);
        dc=GetDC(wg_present_window);
        if(!dc) { ++wg_present_failures;continue; }
        if(width!=bw || height!=bh) {
            HBITMAP probe;
            struct { BITMAPINFOHEADER header; DWORD masks[256]; } format;
            if(upload && upload_original && upload_original!=HGDI_ERROR)SelectObject(upload,upload_original);
            if(compatible)DeleteObject(compatible);
            if(upload)DeleteDC(upload);
            upload=NULL;compatible=NULL;upload_original=NULL;
            if(memory && original && original!=HGDI_ERROR)SelectObject(memory,original);
            if(bitmap)DeleteObject(bitmap);
            if(memory)DeleteDC(memory);
            free(xmap);xmap=NULL;bitmap=NULL;memory=NULL;original=NULL;bits=NULL;
            bw=bh=0;
            ZeroMemory(&info,sizeof(info));
            info.header.biSize=sizeof(info.header);
            info.header.biWidth=width;info.header.biHeight=-height;
            bpp=32;
            wg_desktop_bpp=(unsigned)(GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES));
            ZeroMemory(&format,sizeof(format));format.header.biSize=sizeof(format.header);
            probe=CreateCompatibleBitmap(dc,1,1);
            if(probe && !wg_present_force32 && wg_desktop_bpp==16 &&
                GetDIBits(dc,probe,0,0,NULL,(BITMAPINFO *)&format,DIB_RGB_COLORS)) {
                /* Second query retrieves masks after the format-only query. */
                (void)GetDIBits(dc,probe,0,0,NULL,(BITMAPINFO *)&format,DIB_RGB_COLORS);
                if(format.header.biCompression==BI_RGB) { bpp=16;rgb565=0; }
                else if(format.header.biCompression==BI_BITFIELDS &&
                    format.masks[2]==0x1f && (format.masks[1]==0x7e0 || format.masks[1]==0x3e0) &&
                    format.masks[0]==(format.masks[1]==0x7e0 ? 0xf800U:0x7c00U)) {
                    bpp=16;rgb565=format.masks[1]==0x7e0;
                }
            }
            if(probe)DeleteObject(probe);
            info.header.biPlanes=1;info.header.biBitCount=(WORD)bpp;
            info.header.biCompression=bpp==16 ? BI_BITFIELDS:BI_RGB;
            info.masks[0]=rgb565?0xf800:0x7c00;info.masks[1]=rgb565?0x7e0:0x3e0;info.masks[2]=0x1f;
            if((size_t)width<=((size_t)-1)/4/(size_t)height) {
                memory=CreateCompatibleDC(dc);
                bitmap=CreateDIBSection(dc,(BITMAPINFO *)&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);
                xmap=(unsigned *)malloc((size_t)width*sizeof(*xmap));
            }
            if(memory && bitmap && xmap) {
                original=SelectObject(memory,bitmap);
                if(original && original!=HGDI_ERROR) {
                    bw=width;bh=height;stride=((unsigned)bw*(bpp/8)+3U)&~3U;
                    memset(bits,0,(size_t)stride*bh);
                    mw=0;
                    wg_present_bpp=bpp;
                    if(wg_present_ddb) {
                        upload=CreateCompatibleDC(dc);
                        compatible=CreateCompatibleBitmap(dc,bw,bh);
                        if(upload && compatible)upload_original=SelectObject(upload,compatible);
                    }
                }
            }
        }
        if(!bw || !bh) { ++wg_present_failures;ReleaseDC(wg_present_window,dc);continue; }
        if(memcmp(&last_view,&wg_present_work.view,sizeof(last_view))) {
            memset(bits,0,(size_t)stride*bh);
            last_view=wg_present_work.view;
        }
        if(mw!=vw) { WG_NearestMap(xmap,(unsigned)vw,WOLF3D_SCREEN_WIDTH);mw=vw; }
        if(bpp==16)
            WG_NearestScale16((uint16_t *)((uint8_t *)bits+(size_t)wg_present_work.view.top*stride)+wg_present_work.view.left,
                stride/2,(unsigned)vw,(unsigned)vh,wg_present_work.pixels,
                WOLF3D_SCREEN_WIDTH,WOLF3D_SCREEN_HEIGHT,wg_present_work.palette,xmap,rgb565);
        else
            WG_NearestScale((uint32_t *)((uint8_t *)bits+(size_t)wg_present_work.view.top*stride)+wg_present_work.view.left,
                stride/4,(unsigned)vw,(unsigned)vh,wg_present_work.pixels,
                WOLF3D_SCREEN_WIDTH,WOLF3D_SCREEN_HEIGHT,wg_present_work.palette,xmap);
        if(timed)timed=QueryPerformanceCounter(&blit);
        if(wg_present_ddb && upload && compatible && upload_original && upload_original!=HGDI_ERROR) {
            SelectObject(upload,upload_original);
            if(!SetDIBits(dc,compatible,0,(UINT)bh,bits,(BITMAPINFO *)&info,DIB_RGB_COLORS))++wg_present_failures;
            SelectObject(upload,compatible);
            if(!BitBlt(dc,0,0,bw,bh,upload,0,0,SRCCOPY))++wg_present_failures;
        } else {
            if(wg_present_ddb)++wg_present_failures;
            if(!BitBlt(dc,0,0,bw,bh,memory,0,0,SRCCOPY))++wg_present_failures;
        }
        /* Flush on this worker before reusing DIB memory; never on engine thread. */
        GdiFlush();
        if(timed)timed=QueryPerformanceCounter(&end);
        ReleaseDC(wg_present_window,dc);
        if(timed && QueryPerformanceCounter(&done) && done.QuadPart>=start.QuadPart
            && end.QuadPart>=blit.QuadPart) {
            uint64_t total=(uint64_t)(done.QuadPart-start.QuadPart);
            uint64_t draw=(uint64_t)(end.QuadPart-blit.QuadPart);
            ++wg_present_draws;wg_present_total+=total;wg_present_blits+=draw;
            if(total>wg_present_longest)wg_present_longest=total;
            if(draw>wg_present_blit_longest)wg_present_blit_longest=draw;
            wg_present_width=(unsigned)vw;wg_present_height=(unsigned)vh;
        }
    }
    if(memory && original && original!=HGDI_ERROR)SelectObject(memory,original);
    if(bitmap)DeleteObject(bitmap);
    if(memory)DeleteDC(memory);
    if(upload && upload_original && upload_original!=HGDI_ERROR)SelectObject(upload,upload_original);
    if(compatible)DeleteObject(compatible);
    if(upload)DeleteDC(upload);
    free(xmap);
    return 0;
}
static int WG_PresenterStart(HWND window)
{
    const char *trace=getenv("WOLF3D_OPL_TIMING");
    const char *path=getenv("WOLF3D_GDI_BITMAP"), *format=getenv("WOLF3D_GDI_FORMAT");
    wg_present_ddb=path && strcmp(path,"ddb")==0;
    wg_present_force32=format && strcmp(format,"32")==0;
    wg_present_bpp=wg_desktop_bpp=0;
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
            fprintf(file,"Async GDI presentation (whole window lifetime): nearest-neighbor DIB + unscaled BitBlt\n");
            fprintf(file,"Bitmap path=%s; desktop=%u bpp; source=%u bpp; scaling map cached; borders cleared only on resize\n",
                wg_present_ddb?"ddb upload":"dib",wg_desktop_bpp,wg_present_bpp);
            fprintf(file,"Published=%.0f; replaced pending=%.0f; drawn=%.0f; failures=%.0f; last destination=%ux%u\n",
                (double)wg_present_published,(double)wg_present_replaced,(double)wg_present_draws,
                (double)wg_present_failures,wg_present_width,wg_present_height);
            fprintf(file,"Worker presentation: total=%.6f s; average=%.3f ms; longest=%.3f ms\n",
                (double)wg_present_total/hz,wg_present_draws ? (double)wg_present_total*1000/hz/wg_present_draws:0,
                (double)wg_present_longest*1000/hz);
            fprintf(file,"Unscaled BitBlt + worker GdiFlush: total=%.6f s; longest=%.3f ms\n\n",
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
