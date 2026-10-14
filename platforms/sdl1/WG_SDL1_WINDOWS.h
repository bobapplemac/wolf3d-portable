/* Windows owns all SDL video/input calls on one thread. The engine only
   exchanges copied frames/events; a blocked DirectDraw blit cannot hold it. */
static HANDLE ui_thread,ui_ready,ui_wake;
static CRITICAL_SECTION ui_lock;
static int ui_stop,ui_ok,ui_frame,ui_capture=-1,ui_title;
static char ui_caption[256];
static uint32_t ui_devices;
static uint8_t ui_pixels[64000],ui_palette[768],ui_work_pixels[64000],ui_work_palette[768];
static wolf3d_event_t ui_events[256];
static unsigned ui_read,ui_count;
static DWORD WINAPI ui_run(LPVOID unused)
{
    (void)unused;
    ui_ok=init();ui_devices=ui_ok?devices():0;SetEvent(ui_ready);
    if(!ui_ok)return 0;
    for(;;) {
        int draw,grab,caption,stop,room;
        char text[256];wolf3d_event_t event;
        EnterCriticalSection(&ui_lock);
        stop=ui_stop;draw=ui_frame;ui_frame=0;
        grab=ui_capture;ui_capture=-1;caption=ui_title;ui_title=0;
        if(caption)memcpy(text,ui_caption,sizeof(text));
        if(draw) {memcpy(ui_work_pixels,ui_pixels,sizeof(ui_pixels));memcpy(ui_work_palette,ui_palette,sizeof(ui_palette));}
        LeaveCriticalSection(&ui_lock);
        if(stop)break;
        if(grab>=0)mouse_capture(grab);
        if(caption)title(text);
        for(;;) {
            EnterCriticalSection(&ui_lock);room=ui_count<256;LeaveCriticalSection(&ui_lock);
            if(!room || !poll_event(&event))break;
            EnterCriticalSection(&ui_lock);ui_events[(ui_read+ui_count)%256]=event;++ui_count;LeaveCriticalSection(&ui_lock);
        }
        if(draw)present(ui_work_pixels,ui_work_palette);
        /* Frames and commands wake immediately; idle input polls at 125 Hz. */
        WaitForSingleObject(ui_wake,8);
    }
    shutdown_host();return 0;
}
static int ui_init(void)
{
    ui_stop=ui_ok=ui_frame=ui_title=0;ui_capture=-1;ui_read=ui_count=0;
    InitializeCriticalSection(&ui_lock);
    ui_ready=CreateEvent(NULL,FALSE,FALSE,NULL);ui_wake=CreateEvent(NULL,FALSE,FALSE,NULL);
    ui_thread=(ui_ready && ui_wake)?CreateThread(NULL,0,ui_run,NULL,0,NULL):NULL;
    if(ui_thread) {
        WaitForSingleObject(ui_ready,INFINITE);
        if(ui_ok) {SetThreadPriority(ui_thread,THREAD_PRIORITY_BELOW_NORMAL);return 1;}
        WaitForSingleObject(ui_thread,INFINITE);CloseHandle(ui_thread);ui_thread=NULL;
    }
    if(ui_ready)CloseHandle(ui_ready);
    if(ui_wake)CloseHandle(ui_wake);
    DeleteCriticalSection(&ui_lock);return 0;
}
static void ui_shutdown(void)
{
    if(!ui_thread)return;
    pcm_shutdown();
    EnterCriticalSection(&ui_lock);ui_stop=1;LeaveCriticalSection(&ui_lock);SetEvent(ui_wake);
    WaitForSingleObject(ui_thread,INFINITE);CloseHandle(ui_thread);ui_thread=NULL;
    CloseHandle(ui_ready);CloseHandle(ui_wake);DeleteCriticalSection(&ui_lock);
}
static void ui_present(const uint8_t *pixels,const uint8_t *palette)
{
    if(!ui_thread)return;
    EnterCriticalSection(&ui_lock);memcpy(ui_pixels,pixels,sizeof(ui_pixels));memcpy(ui_palette,palette,sizeof(ui_palette));ui_frame=1;LeaveCriticalSection(&ui_lock);SetEvent(ui_wake);
}
static int ui_poll(wolf3d_event_t *event)
{
    int found;
    if(!ui_thread)return 0;
    EnterCriticalSection(&ui_lock);found=ui_count!=0;
    if(found) {*event=ui_events[ui_read];ui_read=(ui_read+1)%256;--ui_count;}
    LeaveCriticalSection(&ui_lock);return found;
}
static void ui_set_title(const char *text)
{
    if(!ui_thread)return;
    EnterCriticalSection(&ui_lock);strncpy(ui_caption,text,sizeof(ui_caption)-1);ui_caption[sizeof(ui_caption)-1]=0;ui_title=1;LeaveCriticalSection(&ui_lock);SetEvent(ui_wake);
}
static void ui_set_capture(int value)
{
    if(!ui_thread)return;
    EnterCriticalSection(&ui_lock);ui_capture=value!=0;LeaveCriticalSection(&ui_lock);SetEvent(ui_wake);
}
static uint32_t ui_get_devices(void) {return ui_devices;}
