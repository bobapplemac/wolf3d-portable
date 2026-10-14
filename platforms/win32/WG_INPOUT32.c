/* Optional NT transport. Loading InpOut32 may install its embedded driver;
   only an explicit native AdLib request reaches WG_InpOutOpen. */
#include <windows.h>
#include <string.h>
#include "WG_INPOUT32.h"

typedef short (__stdcall *wg_inp_fn)(short);
typedef void (__stdcall *wg_out_fn)(short, short);
typedef BOOL (__stdcall *wg_open_fn)(void);
static HMODULE wg_io_module;
static wg_inp_fn wg_io_read;
static wg_out_fn wg_io_write;
static LARGE_INTEGER wg_io_frequency;
static const char *wg_io_status = "not probed (use --diag --opl adlib)";

void WG_InpOutError(const char *message) { wg_io_status = message; }
const char *WG_InpOutStatus(void) { return wg_io_status; }
void WG_InpOutClose(void)
{
    wg_io_read = NULL;
    wg_io_write = NULL;
    if (wg_io_module) FreeLibrary(wg_io_module);
    wg_io_module = NULL;
}
int WG_InpOutOpen(void)
{
    char path[MAX_PATH];
    char *slash;
    DWORD length;
    FARPROC proc;
    wg_open_fn driver_open;
    LARGE_INTEGER counter;
    if (wg_io_module) return 1;
    if (GetVersion() & 0x80000000UL)
    {
        WG_InpOutError("InpOut32 transport requires Windows NT/2000/XP");
        return 0;
    }
    if (!QueryPerformanceFrequency(&wg_io_frequency) ||
        wg_io_frequency.QuadPart <= 0 || !QueryPerformanceCounter(&counter))
    {
        WG_InpOutError("high-resolution timing unavailable; use DBOPL");
        return 0;
    }
    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!length || length >= sizeof(path))
    {
        WG_InpOutError("cannot locate executable directory");
        return 0;
    }
    slash = strrchr(path, '\\');
    if (!slash || (size_t)(slash + 1 - path) + sizeof("inpout32.dll") > sizeof(path))
    {
        WG_InpOutError("executable directory path is too long");
        return 0;
    }
    strcpy(slash + 1, "inpout32.dll");
    /* Absolute path: never search the current directory or PATH for this DLL. */
    wg_io_module = LoadLibraryA(path);
    if (!wg_io_module)
    {
        WG_InpOutError("cannot load inpout32.dll beside wolf3d.exe (or a DLL dependency is missing)");
        return 0;
    }
    proc = GetProcAddress(wg_io_module, "Inp32");
    memcpy(&wg_io_read, &proc, sizeof(wg_io_read));
    proc = GetProcAddress(wg_io_module, "Out32");
    memcpy(&wg_io_write, &proc, sizeof(wg_io_write));
    proc = GetProcAddress(wg_io_module, "IsInpOutDriverOpen");
    memcpy(&driver_open, &proc, sizeof(driver_open));
    if (!wg_io_read || !wg_io_write || !driver_open)
    {
        WG_InpOutError("inpout32.dll is missing required exports");
        WG_InpOutClose();
        return 0;
    }
    if (!driver_open())
    {
        WG_InpOutError("InpOut driver is not open; install it as Administrator");
        WG_InpOutClose();
        return 0;
    }
    WG_InpOutError("driver open; probing OPL at 0x388");
    return 1;
}
int WG_InpOutRead(unsigned port)
{
    if (!wg_io_read || port < 0x388U || port > 0x38bU) return 0xff;
    return (unsigned char)wg_io_read((short)port);
}
int WG_InpOutWrite(unsigned port, int value)
{
    if (wg_io_write && port >= 0x388U && port <= 0x38bU)
        wg_io_write((short)port, (short)(unsigned char)value);
    return value;
}
/* Only used after a broken performance counter. Two coarse clock ticks
   are not reliably expressible as Sleep(1), so measure a generous interval. */
static void WG_InpOutFallbackWait(void)
{
    DWORD start = GetTickCount();
    do { Sleep(1); } while (GetTickCount() - start < 100U);
}
void WG_InpOutWait(unsigned microseconds)
{
    LARGE_INTEGER start, now;
    LONGLONG ticks;
    DWORD started;
    unsigned spins = 0;
    /* Start AFTER the synchronous port write returns. Round upward and add
       a counter tick so quantization cannot shorten the requested interval.
       This is chip recovery time, not the music sequencer's event delay. */
    ticks = (wg_io_frequency.QuadPart * microseconds + 999999) / 1000000 + 1;
    if (!QueryPerformanceCounter(&start)) { WG_InpOutFallbackWait(); return; }
    started = GetTickCount();
    for (;;)
    {
        if (!QueryPerformanceCounter(&now) || now.QuadPart < start.QuadPart)
        {
            WG_InpOutFallbackWait();
            return;
        }
        if (now.QuadPart - start.QuadPart >= ticks) return;
        /* Avoid hanging forever on a stuck legacy performance counter. */
        if (++spins == 256U)
        {
            spins = 0;
            if (GetTickCount() - started >= 100U) return;
        }
    }
}
