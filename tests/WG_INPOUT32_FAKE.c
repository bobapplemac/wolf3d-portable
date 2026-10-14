/* Integration fixture: no real port access, never distribute this DLL. */
#include <windows.h>
static short address;
static int timer;
__declspec(dllexport) void __stdcall Out32(short port, short value)
{
    if (port == 0x388) address = value;
    if (port == 0x389 && address == 4) timer = value == 0x21;
}
__declspec(dllexport) short __stdcall Inp32(short port)
{
    return port == 0x388 && timer ? 0xc0 : 0;
}
__declspec(dllexport) BOOL __stdcall IsInpOutDriverOpen(void) { return TRUE; }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
    {
        HANDLE marker = CreateFileA("inpout-loaded.marker", GENERIC_WRITE, 0,
                                    NULL, CREATE_ALWAYS, 0, NULL);
        if (marker != INVALID_HANDLE_VALUE) CloseHandle(marker);
    }
    return TRUE;
}
