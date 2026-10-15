#undef NDEBUG
#include <windows.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int accepted,sets,fail_set;
static WPARAM last_key;
static LPARAM last_flags;
static LRESULT CALLBACK original(HWND window,UINT message,WPARAM key,LPARAM flags)
{
    (void)window;last_flags=flags;
    /* SDL1 windib rejects repeat-marked key-downs before its event queue. */
    if((message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && !(flags&((LPARAM)1UL<<30))) {
        ++accepted;last_key=key;
    }
    return 123;
}
static WNDPROC installed=original;
static LONG_PTR WINAPI set_proc(HWND window,int index,LONG_PTR proc)
{
    WNDPROC previous=installed;(void)window;(void)index;
    ++sets;if(fail_set)return 0;installed=(WNDPROC)proc;return (LONG_PTR)previous;
}
static LRESULT WINAPI call_proc(WNDPROC proc,HWND window,UINT message,WPARAM key,LPARAM flags)
{assert(proc==original);return proc(window,message,key,flags);}
#ifdef _WIN64
#define SetWindowLongPtrA set_proc
#else
#define SetWindowLongA set_proc
#endif
#define CallWindowProcA call_proc
#include "../platforms/sdl1/WG_SDL1_WINKEYS.h"
int main(void)
{
    HWND window=(HWND)(ULONG_PTR)1;
    LPARAM repeat=((LPARAM)1UL<<30);
    assert(WG_SDL1KeyAttach(window));assert(sets==1);
    assert(WG_SDL1KeyAttach(window));assert(sets==1);
    installed(window,WM_KEYDOWN,VK_RETURN,0x001c0001);
    installed(window,WM_KEYUP,VK_RETURN,(LPARAM)0xc01c0001UL);
    assert(installed(window,WM_KEYDOWN,'Y',0x40150001)==123);
    assert(accepted==2 && last_key=='Y' && last_flags==0x00150001);
    installed(window,WM_KEYDOWN,'Y',repeat|1);assert(accepted==2);
    installed(window,WM_KEYUP,'Y',repeat|1);
    installed(window,WM_KEYDOWN,'Y',repeat|1);assert(accepted==3);
    installed(window,WM_KILLFOCUS,0,0);
    installed(window,WM_SYSKEYDOWN,'Y',repeat|1);assert(accepted==4);
    installed(window,WM_SYSKEYUP,'Y',repeat|1);
    installed(window,WM_SYSKEYDOWN,'Y',repeat|1);assert(accepted==5);
    WG_SDL1KeyDetach();assert(installed==original && !wg_sdl_key_window);
    WG_SDL1KeyDetach();assert(sets==2);
    /* Recreation restores the original procedure and forgets old key state. */
    assert(WG_SDL1KeyAttach(window));
    installed(window,WM_KEYDOWN,'Y',repeat|1);assert(accepted==6);
    WG_SDL1KeyDetach();assert(installed==original);
    fail_set=1;assert(!WG_SDL1KeyAttach(window));
    assert(!wg_sdl_key_window && !wg_sdl_key_previous && installed==original);
    puts("SDL1 Win9x keys: first press, repeats, release, focus and hook lifecycle passed");
    return 0;
}
