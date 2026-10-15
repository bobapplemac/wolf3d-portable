#undef NDEBUG
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static DWORD clock_ms, initial_clock;
static int stuck, prompted, flushed, reads, writes, restores, mode_failure;
static INPUT_RECORD events[8];
static unsigned event_count;
static DWORD WINAPI test_ticks(void) {return clock_ms;}
static void WINAPI test_sleep(DWORD ms)
{clock_ms+=ms;assert((DWORD)(clock_ms-initial_clock)<=250U);}
static SHORT WINAPI test_key(int key)
{return stuck && (key=='Y' || key==0xff)?(SHORT)0x8000:0;}
static BOOL WINAPI test_get_mode(HANDLE h,LPDWORD mode)
{(void)h;*mode=ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT;return TRUE;}
static BOOL WINAPI test_set_mode(HANDLE h,DWORD mode)
{(void)h;if(mode_failure)return FALSE;if(mode&ENABLE_LINE_INPUT)++restores;return TRUE;}
static BOOL WINAPI test_flush(HANDLE h) {(void)h;flushed=1;return TRUE;}
static BOOL WINAPI test_write(HANDLE h,LPCVOID data,DWORD size,LPDWORD count,LPOVERLAPPED o)
{(void)h;(void)o;assert(flushed);assert(strstr((const char *)data,"Press any key"));prompted=1;++writes;*count=size;return TRUE;}
static BOOL WINAPI test_read(HANDLE h,PINPUT_RECORD event,DWORD size,LPDWORD count)
{(void)h;assert(prompted && size==1);assert((unsigned)reads<event_count);*event=events[reads++];*count=1;return TRUE;}
#define GetTickCount test_ticks
#define Sleep test_sleep
#define GetAsyncKeyState test_key
#define GetConsoleMode test_get_mode
#define SetConsoleMode test_set_mode
#define FlushConsoleInputBuffer test_flush
#define WriteFile test_write
#define ReadConsoleInputA test_read
#include "../platforms/win32/WG_WIN32_CONSOLE.h"
static void reset(int held)
{stuck=held;prompted=flushed=reads=writes=restores=mode_failure=0;event_count=0;initial_clock=clock_ms;}
static void key(WORD code,BOOL down)
{INPUT_RECORD *e=&events[event_count++];memset(e,0,sizeof(*e));e->EventType=KEY_EVENT;e->Event.KeyEvent.wVirtualKeyCode=code;e->Event.KeyEvent.bKeyDown=down;}
int main(void)
{
    reset(1);key('Y',TRUE);key('A',TRUE);
    WG_PauseConsole(1,0);
    assert(prompted && reads==2 && restores==1 && clock_ms-initial_clock==250U);
    /* The original held key becomes usable again after release. */
    reset(1);key('Y',TRUE);key('Y',FALSE);key('Y',TRUE);
    WG_PauseConsole(1,0);assert(reads==3 && restores==1);
    /* DWORD timer wrap must not turn the bounded wait into a hang. */
    clock_ms=0xfffffff0UL;reset(1);key('A',TRUE);
    WG_PauseConsole(1,0);assert((DWORD)(clock_ms-initial_clock)==250U);
    reset(0);key('Y',TRUE);WG_PauseConsole(1,0);
    assert(reads==1 && clock_ms==initial_clock && restores==1);
    reset(1);WG_PauseConsole(0,0);WG_PauseConsole(1,1);assert(!writes && !reads);
    reset(0);mode_failure=1;WG_PauseConsole(1,0);assert(!writes && !reads);
    puts("Console pause: stale keys, held repeats, release, timer wrap and bypass passed");
    return 0;
}
