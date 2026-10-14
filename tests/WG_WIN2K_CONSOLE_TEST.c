/* Real console handles, not redirected pipes: test launcher inheritance and
   the shared B800 renderer in an isolated, hidden console. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../platforms/WG_TEXT_OUTPUT.h"

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
int main(int argc, char **argv)
{
    HANDLE output;
    char command[2048], text[2001];
    COORD origin = {0, 0};
    DWORD count;
    unsigned i;
    unsigned char cells[4000];
    CHAR_INFO cell;
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
        puts("Inherited console help/diagnostics and B800 output passed");
        return 0;
    }
    assert(argc == 3);
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    assert(output != INVALID_HANDLE_VALUE);
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
    return 0;
}

