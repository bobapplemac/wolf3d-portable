#include "WG_HELP.h"

#include "WOLF3D.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

typedef struct wg_help_stream
{
    HANDLE handle;
    int close_handle;
} wg_help_stream_t;

static int WG_AttachHelpConsole(void)
{
    typedef BOOL (WINAPI *wg_attach_console_t)(DWORD);
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    wg_attach_console_t attach_console;
    FARPROC procedure;

    if (kernel == NULL)
    {
        return 0;
    }
    procedure = GetProcAddress(kernel, "AttachConsole");
    memcpy(&attach_console, &procedure, sizeof(attach_console));
    return attach_console != NULL && attach_console((DWORD)-1);
}

static wg_help_stream_t WG_OpenHelpStream(void)
{
    int attached = WG_AttachHelpConsole();
    wg_help_stream_t stream;

    stream.handle = attached ? CreateFileA(
        "CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL) : GetStdHandle(STD_OUTPUT_HANDLE);
    stream.close_handle = attached;

    if (!attached
        && (stream.handle == NULL || stream.handle == INVALID_HANDLE_VALUE))
    {
        if (AllocConsole())
        {
            stream.handle = CreateFileA(
                "CONOUT$", GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
            stream.close_handle = 1;
        }
    }
    if (attached && stream.handle != NULL
        && stream.handle != INVALID_HANDLE_VALUE)
    {
        DWORD written;

        (void)WriteFile(stream.handle, "\n", 1U, &written, NULL);
    }
    return stream;
}

static int WG_HelpStreamValid(const wg_help_stream_t *stream)
{
    return stream != NULL && stream->handle != NULL
        && stream->handle != INVALID_HANDLE_VALUE;
}

static void WG_HelpWrite(wg_help_stream_t *stream, const char *text)
{
    DWORD written;

    if (WG_HelpStreamValid(stream) && text != NULL)
    {
        (void)WriteFile(stream->handle, text, (DWORD)strlen(text),
                        &written, NULL);
    }
}

static void WG_CloseHelpStream(wg_help_stream_t *stream)
{
    if (WG_HelpStreamValid(stream) && stream->close_handle)
    {
        (void)CloseHandle(stream->handle);
    }
}
#else
typedef struct wg_help_stream
{
    FILE *stream;
} wg_help_stream_t;

static wg_help_stream_t WG_OpenHelpStream(void)
{
    wg_help_stream_t stream;

    stream.stream = stdout;
    return stream;
}

static int WG_HelpStreamValid(const wg_help_stream_t *stream)
{
    return stream != NULL && stream->stream != NULL;
}

static void WG_HelpWrite(wg_help_stream_t *stream, const char *text)
{
    if (WG_HelpStreamValid(stream) && text != NULL)
    {
        (void)fputs(text, stream->stream);
    }
}

static void WG_CloseHelpStream(wg_help_stream_t *stream)
{
    if (WG_HelpStreamValid(stream))
    {
        (void)fflush(stream->stream);
    }
}
#endif

int WG_CommandLineHelpRequested(int argc, char **argv)
{
    int index;

    if (argc <= 0 || argv == NULL)
    {
        return 0;
    }
    for (index = 1; index < argc; ++index)
    {
        if (argv[index] != NULL
            && (strcmp(argv[index], "--help") == 0
                || strcmp(argv[index], "-h") == 0
                || strcmp(argv[index], "/?") == 0))
        {
            return 1;
        }
    }
    return 0;
}

void WG_PrintCommandLineHelp(const char *program,
                             const char *host_options,
                             const char *runtime_notes)
{
    wg_help_stream_t stream = WG_OpenHelpStream();
    size_t driver_count;
    size_t index;

    if (!WG_HelpStreamValid(&stream))
    {
        return;
    }
    WG_HelpWrite(&stream, "Usage: ");
    WG_HelpWrite(&stream, program != NULL ? program : "wolf3d");
    WG_HelpWrite(&stream, " [options]\n\n");
    WG_HelpWrite(&stream, wolf3d_GetCommandLineHelp());
    if (host_options != NULL && host_options[0] != '\0')
    {
        WG_HelpWrite(&stream, "\n");
        WG_HelpWrite(&stream, host_options);
    }
    driver_count = wolf3d_GetOPLDriverCount();
    WG_HelpWrite(&stream, "\nCompiled OPL drivers:");
    for (index = 0U; index < driver_count; ++index)
    {
        const char *name = wolf3d_GetOPLDriverName(index);

        if (name != NULL)
        {
            WG_HelpWrite(&stream, " ");
            WG_HelpWrite(&stream, name);
        }
    }
    WG_HelpWrite(&stream, "\n");
    if (runtime_notes != NULL && runtime_notes[0] != '\0')
    {
        WG_HelpWrite(&stream, "\n");
        WG_HelpWrite(&stream, runtime_notes);
    }
    WG_CloseHelpStream(&stream);
}
