/* Real console handles, not redirected pipes: test launcher inheritance and
   the shared B800 renderer in an isolated, hidden console. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../platforms/WG_TEXT_OUTPUT.h"

static int legacy_detection;
static FARPROC WINAPI test_lookup(HMODULE module, LPCSTR name)
{
    return legacy_detection ? NULL : GetProcAddress(module, name);
}
#define GetProcAddress test_lookup
#include "../platforms/win32/WG_WIN32_CONSOLE.h"
#undef GetProcAddress

static char log_path[MAX_PATH + 8];
static void failure(int line)
{
    FILE *log = fopen(log_path, "w");
    if (log) { fprintf(log, "Console assertion failed at line %d (Windows error %lu)\n", line, (unsigned long)GetLastError()); fclose(log); }
    ExitProcess(1);
}
#undef assert
#define assert(expr) ((expr) ? (void)0 : failure(__LINE__))
static int run(const char *command, DWORD flags, int inherit)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char line[2048];
    DWORD code = 99;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    strcpy(line, command);
    if (!CreateProcessA(NULL, line, NULL, NULL, inherit, flags,
                        NULL, NULL, &startup, &process)) return 98;
    if (WaitForSingleObject(process.hProcess, 15000) == WAIT_OBJECT_0)
        GetExitCodeProcess(process.hProcess, &code);
    else
        TerminateProcess(process.hProcess, 99);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return (int)code;
}
static void clear(HANDLE output)
{
    COORD origin = {0, 0};
    DWORD written;
    assert(FillConsoleOutputCharacterA(output, ' ', 2000, origin, &written));
    assert(SetConsoleCursorPosition(output, origin));
}
static volatile LONG fresh_key_sent;
static DWORD WINAPI dismiss_prompt(LPVOID parameter)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    COORD origin = {0, 0};
    INPUT_RECORD event;
    char text[2001];
    DWORD count;
    unsigned attempt;
    (void)parameter;
    for (attempt = 0; attempt < 100; ++attempt)
    {
        assert(ReadConsoleOutputCharacterA(output, text, 2000, origin, &count));
        text[count] = 0;
        if (strstr(text, "Press any key to close...") != NULL)
        {
            ZeroMemory(&event, sizeof(event));
            event.EventType = KEY_EVENT;
            event.Event.KeyEvent.bKeyDown = TRUE;
            event.Event.KeyEvent.wVirtualKeyCode = 'A';
            event.Event.KeyEvent.wRepeatCount = 1;
            Sleep(100);
            InterlockedExchange(&fresh_key_sent, 1);
            assert(WriteConsoleInputA(input, &event, 1, &count));
            return 0;
        }
        Sleep(10);
    }
    failure(__LINE__);
    return 1;
}

int main(int argc, char **argv)
{
    HANDLE output;
    char command[2048], text[2001];
    COORD origin = {0, 0};
    DWORD count;
    unsigned i;
    unsigned char cells[4000];
    CHAR_INFO cell;
    HANDLE thread;
    DWORD old_mode, new_mode;
    INPUT_RECORD stale;
    COORD moved = {0, 1};
    char *options[] = {"wolf3d", "--no-pause"};
    COORD size = {1, 1};
    SMALL_RECT region = {0, 0, 0, 0};
    sprintf(log_path, "%s.log", argv[0]);
    if (argc == 2)
    {
        char self[MAX_PATH];
        GetModuleFileNameA(NULL, self, sizeof(self));
        sprintf(command, "\"%s\" --child \"%s\"", self, argv[1]);
        if (run(command, CREATE_NEW_CONSOLE, 0) != 0)
        {
            FILE *log = fopen(log_path, "r");
            if (log) { while (fgets(text, sizeof(text), log)) fputs(text, stderr); fclose(log); }
            return 1;
        }
        puts("Console inheritance, B800 output, detection and exit pause passed");
        return 0;
    }
    assert(argc == 3);
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    assert(output != INVALID_HANDLE_VALUE);
    clear(output);
    assert(WG_OwnConsole());
    legacy_detection = 1;
    assert(WG_OwnConsole());
    assert(SetConsoleCursorPosition(output, moved));
    assert(!WG_OwnConsole());
    legacy_detection = 0;
    clear(output);
    sprintf(command, "\"%s\" --no-config --help", argv[2]);
    assert(run(command, 0, 1) == 0);
    assert(ReadConsoleOutputCharacterA(output, text, 2000, origin, &count));
    text[count] = 0;
    assert(strstr(text, "Usage:") != NULL);
    clear(output);
    sprintf(command, "\"%s\" --no-config --diag", argv[2]);
    assert(run(command, 0, 1) == 0);
    assert(ReadConsoleOutputCharacterA(output, text, 2000, origin, &count));
    text[count] = 0;
    assert(strstr(text, "wolf3d portable diagnostics") != NULL);
    clear(output);
    for (i = 0; i < 2000; ++i) { cells[i*2] = ' '; cells[i*2+1] = 7; }
    cells[0] = 'W'; cells[1] = 0x1e;
    WG_WriteWindowsTextScreen(cells, 80, 25);
    assert(ReadConsoleOutputA(output, &cell, size, origin, &region));
    assert(cell.Char.AsciiChar == 'W' && cell.Attributes == 0x1e);
    assert(WG_NoPause(2, options) && !WG_NoPause(1, options));
    WG_PauseConsole(0, 0);
    WG_PauseConsole(1, 1);
    clear(output);
    assert(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &old_mode));
    ZeroMemory(&stale, sizeof(stale));
    stale.EventType = KEY_EVENT;
    stale.Event.KeyEvent.bKeyDown = TRUE;
    stale.Event.KeyEvent.wVirtualKeyCode = 'Q';
    stale.Event.KeyEvent.wRepeatCount = 1;
    assert(WriteConsoleInputA(GetStdHandle(STD_INPUT_HANDLE), &stale, 1, &count));
    thread = CreateThread(NULL, 0, dismiss_prompt, NULL, 0, NULL);
    assert(thread != NULL);
    WG_PauseConsole(1, 0);
    assert(fresh_key_sent);
    assert(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    assert(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &new_mode));
    assert(old_mode == new_mode);
    return 0;
}

