/* Win9x can mark a first WM_KEYDOWN as a repeat. SDL1 windib discards
   that message before producing an SDL event. Track transitions on the
   window thread so only repeats of an observed press retain that flag. */
static HWND wg_sdl_key_window;
static WNDPROC wg_sdl_key_previous;
static unsigned char wg_sdl_key_down[256];
#ifdef _WIN64
#define WG_SDL1_SET_PROC(window, proc) ((WNDPROC)SetWindowLongPtrA(window,GWLP_WNDPROC,(LONG_PTR)(proc)))
#else
#define WG_SDL1_SET_PROC(window, proc) ((WNDPROC)SetWindowLongA(window,GWL_WNDPROC,(LONG)(proc)))
#endif
static LRESULT CALLBACK WG_SDL1KeyProc(HWND window,UINT message,WPARAM key,LPARAM flags)
{
    WNDPROC previous=wg_sdl_key_previous;
    if((message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && key<256U) {
        if(!wg_sdl_key_down[key])flags &= ~((LPARAM)1UL<<30);
        wg_sdl_key_down[key]=1;
    } else if((message==WM_KEYUP || message==WM_SYSKEYUP) && key<256U) {
        wg_sdl_key_down[key]=0;
    } else if(message==WM_KILLFOCUS) {
        memset(wg_sdl_key_down,0,sizeof(wg_sdl_key_down));
    }
    return CallWindowProcA(previous,window,message,key,flags);
}
static void WG_SDL1KeyDetach(void)
{
    if(wg_sdl_key_window)WG_SDL1_SET_PROC(wg_sdl_key_window,wg_sdl_key_previous);
    wg_sdl_key_window=NULL;wg_sdl_key_previous=NULL;
    memset(wg_sdl_key_down,0,sizeof(wg_sdl_key_down));
}
static int WG_SDL1KeyAttach(HWND window)
{
    WNDPROC previous;
    if(window==wg_sdl_key_window)return 1;
    WG_SDL1KeyDetach();
    previous=WG_SDL1_SET_PROC(window,WG_SDL1KeyProc);
    if(!previous)return 0;
    wg_sdl_key_previous=previous;wg_sdl_key_window=window;
    return 1;
}
#undef WG_SDL1_SET_PROC
