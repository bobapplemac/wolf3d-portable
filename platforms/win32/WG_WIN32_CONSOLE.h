#ifndef WG_WIN32_CONSOLE_H
#define WG_WIN32_CONSOLE_H

/* Call before producing any output. XP+ exposes an exact process count;
   Win9x/2000 use the legacy untouched-cursor heuristic. A normal shell prompt
   has already moved the cursor. --no-pause covers unusual empty-console hosts. */
static int WG_OwnConsole(void)
{
    typedef DWORD (WINAPI *wg_console_process_list_t)(LPDWORD, DWORD);
    wg_console_process_list_t process_list;
    FARPROC address;
    HMODULE kernel;
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode, process, count;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleMode(input, &mode) || !GetConsoleMode(output, &mode)) return 0;
    kernel = GetModuleHandleA("kernel32.dll");
    address = kernel ? GetProcAddress(kernel, "GetConsoleProcessList") : NULL;
    memcpy(&process_list, &address, sizeof(process_list));
    if (process_list != NULL)
    {
        count = process_list(&process, 1);
        if (count != 0) return count == 1;
    }
    return GetConsoleScreenBufferInfo(output, &info)
        && info.dwCursorPosition.X == 0 && info.dwCursorPosition.Y == 0;
}

static int WG_NoPause(int argc, char **argv)
{
    int index;
    for (index = 1; index < argc; ++index)
        if (strcmp(argv[index], "--no-pause") == 0) return 1;
    return 0;
}

static void WG_PauseConsole(int owned, int disabled)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD input_mode, output_mode, count, release_start;
    INPUT_RECORD event;
    unsigned char held_keys[256];
    int key, held;
    const char prompt[] = "\nPress any key to close...";
    if (!owned || disabled || !GetConsoleMode(input, &input_mode)
        || !GetConsoleMode(output, &output_mode)) return;
    /* Do not let the key used to leave the game, or queued console events,
       dismiss the exit card. Mouse buttons are not keyboard keys. */
    memset(held_keys, 0, sizeof(held_keys));
    release_start = GetTickCount();
    do
    {
        held = 0;
        for (key = 8; key < 256; ++key)
        {
            held_keys[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
            if (held_keys[key]) held = 1;
        }
        /* A stale/unsupported key state must not hide the prompt forever.
           Still suppress repeats from keys held when the grace period ends;
           their console key-up event rearms them without blocking other keys. */
        if ((DWORD)(GetTickCount() - release_start) >= 250U) break;
        if (held) Sleep(10);
    } while (held);
    if (!SetConsoleMode(input, input_mode
            & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT))) return;
    if (!FlushConsoleInputBuffer(input))
    {
        (void)SetConsoleMode(input, input_mode);
        return;
    }
    (void)SetConsoleTextAttribute(output, 7);
    if (WriteFile(output, prompt, sizeof(prompt) - 1, &count, NULL))
    {
        while (ReadConsoleInputA(input, &event, 1, &count) && count != 0)
        {
            if (event.EventType == KEY_EVENT)
            {
                key = event.Event.KeyEvent.wVirtualKeyCode;
                if (!event.Event.KeyEvent.bKeyDown)
                {
                    if (key < 256) held_keys[key] = 0;
                }
                else if (key >= 256 || !held_keys[key]) break;
            }
        }
    }
    (void)SetConsoleMode(input, input_mode);
}
#endif
