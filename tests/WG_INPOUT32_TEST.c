/* No real DLL is loaded and no hardware ports are accessed by this test. */
#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mode, reads, writes, closes, sleeps;
static unsigned address[2], regs[512], timer;
static LONGLONG clock_ticks, last_write;
static unsigned last_port;
static DWORD coarse;
static UINT error_mode = SEM_NOGPFAULTERRORBOX;
static UINT WINAPI test_error_mode(UINT value) { UINT old = error_mode; error_mode = value; return old; }
static DWORD WINAPI test_error(void) { return mode == 10 ? ERROR_BAD_EXE_FORMAT : ERROR_MOD_NOT_FOUND; }
static DWORD WINAPI test_attributes(LPCSTR path) { (void)path; return mode == 1 ? INVALID_FILE_ATTRIBUTES : FILE_ATTRIBUTE_NORMAL; }
static short __stdcall mock_in(short port)
{
    assert(port == 0x388); ++reads;
    return mode == 4 ? 0xff : (timer ? 0xc0 : 0);
}
static void __stdcall mock_out(short port, short value)
{
    unsigned bank = ((unsigned)port - 0x388) / 2;
    assert(port >= 0x388 && port <= 0x38b);
    if (writes)
        assert(clock_ticks - last_write >= ((last_port & 1) ? 24 : 4));
    last_write = clock_ticks; last_port = (unsigned)port; ++writes;
    if (!(port & 1)) address[bank] = (unsigned)value;
    else { regs[bank * 256 + address[bank]] = (unsigned)value;
           if (!bank && address[0] == 4) timer = value == 0x21; }
}
static BOOL __stdcall mock_open(void) { return mode != 3; }
static DWORD WINAPI test_version(void) { return mode == 7 ? 0x80000004UL : 5; }
static DWORD WINAPI test_path(HMODULE module, LPSTR path, DWORD length)
{
    (void)module; (void)length;
    strcpy(path, "C:\\Games\\wolf3d.exe"); return (DWORD)strlen(path);
}
static HMODULE WINAPI test_load(LPCSTR path)
{
    #ifdef _WIN64
    assert(!strcmp(path, "C:\\Games\\inpoutx64.dll"));
#else
    assert(!strcmp(path, "C:\\Games\\inpout32.dll"));
#endif
    assert((error_mode & (SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX)) ==
           (SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX));
    assert(error_mode & SEM_NOGPFAULTERRORBOX);
    return mode == 1 || mode == 10 || mode == 11 ? NULL : (HMODULE)1;
}
static BOOL WINAPI test_free(HMODULE module) { assert(module); ++closes; return TRUE; }
static FARPROC WINAPI test_proc(HMODULE module, LPCSTR name)
{
    FARPROC result = NULL;
    (void)module;
    if (mode == 2) return result;
    if (!strcmp(name, "Inp32")) { short (__stdcall *p)(short) = mock_in; memcpy(&result, &p, sizeof(p)); }
    if (!strcmp(name, "Out32")) { void (__stdcall *p)(short, short) = mock_out; memcpy(&result, &p, sizeof(p)); }
    if (!strcmp(name, "IsInpOutDriverOpen")) { BOOL (__stdcall *p)(void) = mock_open; memcpy(&result, &p, sizeof(p)); }
    return result;
}
static BOOL WINAPI test_frequency(LARGE_INTEGER *value) { value->QuadPart = 1000000; return mode != 5; }
static BOOL WINAPI test_counter(LARGE_INTEGER *value)
{
    value->QuadPart = mode == 8 ? 0 : (mode == 9 ? -clock_ticks++ : clock_ticks++);
    return mode != 6;
}
static DWORD WINAPI test_ticks(void) { return coarse++; }
static void WINAPI test_sleep(DWORD ms) { coarse += ms; ++sleeps; }
#define GetVersion test_version
#define GetModuleFileNameA test_path
#define LoadLibraryA test_load
#define SetErrorMode test_error_mode
#define GetLastError test_error
#define GetFileAttributesA test_attributes
#define FreeLibrary test_free
#define GetProcAddress test_proc
#define QueryPerformanceFrequency test_frequency
#define QueryPerformanceCounter test_counter
#define GetTickCount test_ticks
#define Sleep test_sleep
#include "../platforms/win32/WG_INPOUT32.c"
#define WG_INPOUT32 1
#include "../platforms/dos/WG_DOS_ADLIB.c"
static void reset(unsigned failure)
{
    WG_DOSAdLibShutdown(); WG_InpOutClose();
    mode = failure; reads = writes = closes = sleeps = timer = 0;
    clock_ticks = last_write = 0; coarse = 0;
    memset(regs, 0, sizeof(regs));
}
int main(void)
{
    unsigned i, before;
    for (i = 1; i <= 7; ++i)
    {
        reset(i); assert(!WG_DOSAdLibInit()); assert(!wg_io_module);
        if (i != 4) assert(!writes && !reads);
        if (i == 4) assert(strstr(WG_InpOutStatus(), "chip did not respond"));
    }
    assert(error_mode == SEM_NOGPFAULTERRORBOX);
    reset(10); assert(!WG_DOSAdLibInit()); assert(strstr(WG_InpOutStatus(), "incompatible"));
    reset(11); assert(!WG_DOSAdLibInit()); assert(strstr(WG_InpOutStatus(), "dependency"));
    assert(error_mode == SEM_NOGPFAULTERRORBOX);
    reset(0); assert(WG_DOSAdLibInit());
    assert(reads == 2); /* All recovery delays use elapsed time, not port reads. */
    assert(regs[0x105] == 0 && regs[0x104] == 0 && regs[1] == 0x20);
    before = writes; assert(WG_DOSAdLibInit() && writes == before);
    WG_DOSAdLibWrite(0xc0, 3); assert(writes == before + 2 && regs[0xc0] == 3);
    WG_DOSAdLibWrite(0x105, 1); assert(writes == before + 2);
    WG_InpOutWrite(0x220, 1); assert(writes == before + 2);
    WG_DOSAdLibShutdown(); assert(closes == 1 && !wg_io_module);
    before = writes; WG_DOSAdLibShutdown(); assert(writes == before);
    mode = 6; WG_InpOutWait(24); assert(sleeps && coarse >= 100);
    mode = 8; coarse = 0; WG_InpOutWait(24); assert(coarse >= 100);
    mode = 9; coarse = 0; WG_InpOutWait(24); assert(coarse >= 100);
    mode = 0; clock_ticks = 0; wg_io_frequency.QuadPart = 3579545;
    WG_InpOutWait(4); assert(clock_ticks >= 16);
    puts("InpOut: loader failures, driver/chip detection, port bounds, timed writes and cleanup passed.");
    return 0;
}
