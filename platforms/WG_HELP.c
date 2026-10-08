#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "WG_HELP.h"

#include "WOLF3D.h"

#include <stdio.h>
#include <stdlib.h>
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
#include <dirent.h>
#include <sys/stat.h>

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

int WG_CommandLineDiagnosticsRequested(int argc, char **argv)
{
    int index;

    if (argc <= 0 || argv == NULL)
    {
        return 0;
    }
    for (index = 1; index < argc; ++index)
    {
        if (argv[index] != NULL && strcmp(argv[index], "--diag") == 0)
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
    WG_HelpWrite(&stream,
        "Launcher:\n"
        "  --config FILE        Load defaults from a specific argument file\n"
        "  --no-config          Do not load an automatic config file\n"
        "  --diag               Print diagnostics and exit without starting the game\n"
        "  --help, -h, /?       Print this help and exit\n\n");
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

#define WG_DISCOVERY_PROFILE_COUNT 7U
#define WG_DISCOVERY_REQUIRED_FILES 8U

typedef struct wg_discovery_profile
{
    const char *extension;
    const char *map_extension;
    const char *resource_extension;
} wg_discovery_profile_t;

typedef struct wg_discovery_candidate
{
    unsigned profile;
    char *path;
} wg_discovery_candidate_t;

typedef struct wg_discovery_results
{
    wg_discovery_candidate_t *items;
    size_t count;
    size_t capacity;
    int failed;
} wg_discovery_results_t;

static void WG_ErrorAppend(char *error, size_t error_size,
                           const char *text);
static int WG_ProfileFromText(const char *text, unsigned *profile);

static const wg_discovery_profile_t WG_DiscoveryProfiles[] =
{
    { "WL6", "WL6", "WL6" },
    { "WL1", "WL1", "WL1" },
    { "SOD", "SOD", "SOD" },
    { "SDM", "SDM", "SDM" },
    { "SD1", "SD1", "SOD" },
    { "SD2", "SD2", "SOD" },
    { "SD3", "SD3", "SOD" }
};

static const char *const WG_DiscoveryFileNames[] =
{
    "GAMEMAPS", "MAPHEAD", "VSWAP", "VGADICT",
    "VGAGRAPH", "VGAHEAD", "AUDIOHED", "AUDIOT"
};

static int WG_ASCIIEqualCharacter(char left, char right)
{
    if (left >= 'a' && left <= 'z')
    {
        left = (char)(left - ('a' - 'A'));
    }
    if (right >= 'a' && right <= 'z')
    {
        right = (char)(right - ('a' - 'A'));
    }
    return left == right;
}

static int WG_ASCIIEqual(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
    {
        return 0;
    }
    while (*left != '\0' && *right != '\0'
           && WG_ASCIIEqualCharacter(*left, *right))
    {
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static int WG_ASCIIPrefix(const char *text, const char *prefix)
{
    while (*prefix != '\0')
    {
        if (*text == '\0' || !WG_ASCIIEqualCharacter(*text, *prefix))
        {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return 1;
}

static char *WG_CopyString(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL)
    {
        return NULL;
    }
    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy != NULL)
    {
        memcpy(copy, text, length + 1U);
    }
    return copy;
}

static char *WG_JoinPath(const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    int separator = directory_length != 0U
        && directory[directory_length - 1U] != '/'
        && directory[directory_length - 1U] != '\\';
    char *path = (char *)malloc(directory_length + (size_t)separator
                                + name_length + 1U);

    if (path != NULL)
    {
        memcpy(path, directory, directory_length);
        if (separator)
        {
            path[directory_length++] = '/';
        }
        memcpy(path + directory_length, name, name_length + 1U);
    }
    return path;
}

static int WG_FileNameMatches(const char *name, const char *base,
                              const char *extension)
{
    while (*base != '\0')
    {
        if (*name == '\0' || !WG_ASCIIEqualCharacter(*name, *base))
        {
            return 0;
        }
        ++name;
        ++base;
    }
    if (*name++ != '.')
    {
        return 0;
    }
    return WG_ASCIIEqual(name, extension);
}

static void WG_RecordFile(unsigned masks[WG_DISCOVERY_PROFILE_COUNT],
                          const char *name)
{
    unsigned profile;
    unsigned file;

    for (profile = 0U; profile < WG_DISCOVERY_PROFILE_COUNT; ++profile)
    {
        for (file = 0U; file < WG_DISCOVERY_REQUIRED_FILES; ++file)
        {
            const char *extension = file < 3U
                ? WG_DiscoveryProfiles[profile].map_extension
                : WG_DiscoveryProfiles[profile].resource_extension;

            if (WG_FileNameMatches(name, WG_DiscoveryFileNames[file],
                                   extension))
            {
                masks[profile] |= 1U << file;
            }
        }
    }
}

static int WG_AddCandidate(wg_discovery_results_t *results,
                           unsigned profile, const char *path)
{
    wg_discovery_candidate_t *items;
    size_t capacity;

    if (results->count == results->capacity)
    {
        capacity = results->capacity == 0U ? 8U : results->capacity * 2U;
        items = (wg_discovery_candidate_t *)realloc(
            results->items, capacity * sizeof(*items));
        if (items == NULL)
        {
            results->failed = 1;
            return 0;
        }
        results->items = items;
        results->capacity = capacity;
    }
    results->items[results->count].path = WG_CopyString(path);
    if (results->items[results->count].path == NULL)
    {
        results->failed = 1;
        return 0;
    }
    results->items[results->count].profile = profile;
    ++results->count;
    return 1;
}

static void WG_RecordDirectory(wg_discovery_results_t *results,
                               const char *path,
                               const unsigned masks[WG_DISCOVERY_PROFILE_COUNT])
{
    unsigned profile;

    for (profile = 0U; profile < WG_DISCOVERY_PROFILE_COUNT; ++profile)
    {
        if (masks[profile] == 0xffU
            && !WG_AddCandidate(results, profile, path))
        {
            return;
        }
    }
}

#ifdef _WIN32
static void WG_ScanDirectory(const char *path,
                             wg_discovery_results_t *results)
{
    WIN32_FIND_DATAA entry;
    HANDLE search;
    char *pattern;
    unsigned masks[WG_DISCOVERY_PROFILE_COUNT] = { 0U };

    if (results->failed)
    {
        return;
    }
    pattern = WG_JoinPath(path, "*");
    if (pattern == NULL)
    {
        results->failed = 1;
        return;
    }
    search = FindFirstFileA(pattern, &entry);
    free(pattern);
    if (search == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (strcmp(entry.cFileName, ".") == 0
            || strcmp(entry.cFileName, "..") == 0)
        {
            continue;
        }
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)
        {
            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U)
            {
                char *child = WG_JoinPath(path, entry.cFileName);

                if (child == NULL)
                {
                    results->failed = 1;
                    break;
                }
                WG_ScanDirectory(child, results);
                free(child);
            }
        }
        else
        {
            WG_RecordFile(masks, entry.cFileName);
        }
    } while (!results->failed && FindNextFileA(search, &entry));
    (void)FindClose(search);
    if (!results->failed)
    {
        WG_RecordDirectory(results, path, masks);
    }
}
#else
static void WG_ScanDirectory(const char *path,
                             wg_discovery_results_t *results)
{
    DIR *directory;
    struct dirent *entry;
    unsigned masks[WG_DISCOVERY_PROFILE_COUNT] = { 0U };

    if (results->failed)
    {
        return;
    }
    directory = opendir(path);
    if (directory == NULL)
    {
        return;
    }
    while (!results->failed && (entry = readdir(directory)) != NULL)
    {
        char *child;
        struct stat status;

        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        child = WG_JoinPath(path, entry->d_name);
        if (child == NULL)
        {
            results->failed = 1;
            break;
        }
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
        if (lstat(child, &status) == 0)
        {
            if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode))
            {
                WG_ScanDirectory(child, results);
            }
            else if (!S_ISLNK(status.st_mode))
            {
                WG_RecordFile(masks, entry->d_name);
            }
        }
#else
        if (stat(child, &status) == 0 && S_ISDIR(status.st_mode))
        {
            WG_ScanDirectory(child, results);
        }
        else
        {
            WG_RecordFile(masks, entry->d_name);
        }
#endif
        free(child);
    }
    closedir(directory);
    if (!results->failed)
    {
        WG_RecordDirectory(results, path, masks);
    }
}
#endif

static void WG_FreeDiscoveryResults(wg_discovery_results_t *results)
{
    size_t index;

    for (index = 0U; index < results->count; ++index)
    {
        free(results->items[index].path);
    }
    free(results->items);
    memset(results, 0, sizeof(*results));
}

static char *WG_ExecutableDirectory(const char *argument_zero)
{
    const char *separator = NULL;
    const char *cursor;
    size_t length;
    char *directory;

    if (argument_zero == NULL)
    {
        return WG_CopyString(".");
    }
    for (cursor = argument_zero; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\')
        {
            separator = cursor;
        }
    }
    if (separator == NULL)
    {
        return WG_CopyString(".");
    }
    length = (size_t)(separator - argument_zero);
    if (length == 0U || (length == 2U && argument_zero[1] == ':'))
    {
        ++length;
    }
    directory = (char *)malloc(length + 1U);
    if (directory != NULL)
    {
        memcpy(directory, argument_zero, length);
        directory[length] = '\0';
    }
    return directory;
}

static int WG_IsAbsolutePath(const char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\')
    {
        return 1;
    }
    return path[0] != '\0' && path[1] == ':';
}

static int WG_EndsWithASCII(const char *text, const char *suffix)
{
    size_t text_length;
    size_t suffix_length;

    if (text == NULL || suffix == NULL)
    {
        return 0;
    }
    text_length = strlen(text);
    suffix_length = strlen(suffix);
    return text_length >= suffix_length
        && WG_ASCIIEqual(text + text_length - suffix_length, suffix);
}

static char *WG_ConfigPath(const char *argument_zero, int normalized)
{
    static const char *suffixes[] = { "-sdl3", "-win32", "-console" };
#if defined(_WIN32) || defined(__DOS__)
    static const char extension[] = ".ini";
#else
    static const char extension[] = ".conf";
#endif
    const char *base;
    const char *cursor;
    const char *dot;
    char *directory;
    char *stem;
    char *name;
    char *path;
    size_t stem_length;
    size_t index;

    if (argument_zero == NULL)
    {
        return NULL;
    }
    base = argument_zero;
    for (cursor = argument_zero; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\')
        {
            base = cursor + 1;
        }
    }
    dot = strrchr(base, '.');
    stem_length = dot != NULL ? (size_t)(dot - base) : strlen(base);
    stem = (char *)malloc(stem_length + 1U);
    if (stem == NULL)
    {
        return NULL;
    }
    memcpy(stem, base, stem_length);
    stem[stem_length] = '\0';
    if (normalized)
    {
        for (index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]);
             ++index)
        {
            if (WG_EndsWithASCII(stem, suffixes[index]))
            {
                stem[strlen(stem) - strlen(suffixes[index])] = '\0';
                break;
            }
        }
    }
    name = (char *)malloc(strlen(stem) + sizeof(extension));
    if (name == NULL)
    {
        free(stem);
        return NULL;
    }
    strcpy(name, stem);
    strcat(name, extension);
    directory = WG_ExecutableDirectory(argument_zero);
    path = directory != NULL ? WG_JoinPath(directory, name) : NULL;
    free(directory);
    free(name);
    free(stem);
    return path;
}

static const char *WG_OptionGroup(const char *option)
{
    unsigned profile;

    if (option == NULL)
    {
        return "";
    }
    if (strcmp(option, "--game") == 0
        || (option[0] == '-' && option[1] != '-'
            && WG_ProfileFromText(option + 1, &profile)))
    {
        return "game";
    }
    if (strcmp(option, "--mouse") == 0 || strcmp(option, "--nomouse") == 0)
    {
        return "mouse";
    }
    if (strcmp(option, "--joy") == 0 || strcmp(option, "--nojoy") == 0)
    {
        return "joystick";
    }
    if (strcmp(option, "--fullscreen") == 0
        || strcmp(option, "--windowed") == 0)
    {
        return "display-mode";
    }
    if (strcmp(option, "--adlib") == 0
        || strcmp(option, "--pc-speaker") == 0
        || strcmp(option, "--no-sound") == 0
        || WG_ASCIIEqual(option, "-noal")
        || WG_ASCIIEqual(option, "-nosb"))
    {
        return "sound-hardware";
    }
    return option;
}

static int WG_CommandOverrides(int argc, char **argv, const char *option)
{
    const char *group = WG_OptionGroup(option);
    int index;

    for (index = 1; index < argc; ++index)
    {
        if (strcmp(WG_OptionGroup(argv[index]), group) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void WG_FreeTokens(char **tokens, size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        free(tokens[index]);
    }
    free(tokens);
}

static int WG_AddToken(char ***tokens, size_t *count, size_t *capacity,
                       const char *text, size_t length)
{
    char *copy;
    char **grown;
    size_t new_capacity;

    if (*count == *capacity)
    {
        new_capacity = *capacity == 0U ? 16U : *capacity * 2U;
        grown = (char **)realloc(*tokens, new_capacity * sizeof(*grown));
        if (grown == NULL)
        {
            return 0;
        }
        *tokens = grown;
        *capacity = new_capacity;
    }
    copy = (char *)malloc(length + 1U);
    if (copy == NULL)
    {
        return 0;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    (*tokens)[(*count)++] = copy;
    return 1;
}

static int WG_ParseConfigLine(const char *line, char ***tokens,
                              size_t *count, size_t *capacity,
                              size_t *line_start)
{
    const char *cursor = line;

    *line_start = *count;
    while (*cursor == ' ' || *cursor == '\t')
    {
        ++cursor;
    }
    if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n'
        || *cursor == '#' || *cursor == ';')
    {
        return 1;
    }
    while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n')
    {
        char value[4096];
        size_t length = 0U;
        int quoted = 0;

        while (*cursor == ' ' || *cursor == '\t')
        {
            ++cursor;
        }
        if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n')
        {
            break;
        }
        if (*cursor == '"')
        {
            quoted = 1;
            ++cursor;
        }
        while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n'
               && (quoted || (*cursor != ' ' && *cursor != '\t')))
        {
            if (quoted && *cursor == '"')
            {
                ++cursor;
                quoted = 0;
                break;
            }
            if (quoted && *cursor == '\\'
                && (cursor[1] == '"' || cursor[1] == '\\'))
            {
                ++cursor;
            }
            if (length + 1U >= sizeof(value))
            {
                return 0;
            }
            value[length++] = *cursor++;
        }
        if (quoted || !WG_AddToken(tokens, count, capacity, value, length))
        {
            return 0;
        }
    }
    return 1;
}

static int WG_PathOption(const char *option)
{
    return strcmp(option, "--data") == 0
        || strcmp(option, "--drm-device") == 0
        || strcmp(option, "--fb-device") == 0
        || strcmp(option, "--input-device") == 0;
}

int WG_LoadLauncherArguments(int argc, char **argv,
                             wg_launcher_arguments_t *prepared,
                             char *error, size_t error_size)
{
    char *exact_path = NULL;
    char *normal_path = NULL;
    char *selected_path = NULL;
    char *config_directory = NULL;
    char **tokens = NULL;
    size_t token_count = 0U;
    size_t token_capacity = 0U;
    FILE *file = NULL;
    const char *explicit_path = NULL;
    int no_config = 0;
    int index;
    unsigned long line_number = 0UL;
    char line[4096];

    if (prepared == NULL || argc <= 0 || argv == NULL)
    {
        return 0;
    }
    prepared->argc = argc;
    prepared->argv = argv;
    prepared->config_argc = 0;
    prepared->config_path = NULL;
    if (error != NULL && error_size != 0U)
    {
        error[0] = '\0';
    }
    for (index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--no-config") == 0)
        {
            no_config = 1;
        }
        else if (strcmp(argv[index], "--config") == 0)
        {
            if (++index >= argc)
            {
                WG_ErrorAppend(error, error_size,
                               "--config requires a file path.\n");
                return 0;
            }
            explicit_path = argv[index];
        }
    }
    if (no_config)
    {
        return 1;
    }
    if (explicit_path != NULL)
    {
        selected_path = WG_CopyString(explicit_path);
        if (selected_path != NULL)
        {
            file = fopen(selected_path, "r");
        }
        if (file == NULL)
        {
            WG_ErrorAppend(error, error_size, "Unable to open config file: ");
            WG_ErrorAppend(error, error_size, explicit_path);
            WG_ErrorAppend(error, error_size, "\n");
            free(selected_path);
            return 0;
        }
    }
    else
    {
        exact_path = WG_ConfigPath(argv[0], 0);
        normal_path = WG_ConfigPath(argv[0], 1);
        if (exact_path == NULL || normal_path == NULL)
        {
            free(exact_path);
            free(normal_path);
            WG_ErrorAppend(error, error_size,
                           "Unable to allocate the config-file path.\n");
            return 0;
        }
        file = fopen(exact_path, "r");
        if (file != NULL)
        {
            selected_path = exact_path;
            exact_path = NULL;
        }
        else if (!WG_ASCIIEqual(exact_path, normal_path))
        {
            file = fopen(normal_path, "r");
            if (file != NULL)
            {
                selected_path = normal_path;
                normal_path = NULL;
            }
        }
        free(exact_path);
        free(normal_path);
        if (file == NULL)
        {
            return 1;
        }
    }
    config_directory = WG_ExecutableDirectory(selected_path);
    if (config_directory == NULL)
    {
        WG_ErrorAppend(error, error_size,
                       "Unable to allocate the config-file directory.\n");
        goto failure;
    }
    while (fgets(line, sizeof(line), file) != NULL)
    {
        size_t line_start;
        size_t line_end;

        ++line_number;
        if (strchr(line, '\n') == NULL && !feof(file))
        {
            WG_ErrorAppend(error, error_size, "Config line is too long in ");
            WG_ErrorAppend(error, error_size, selected_path);
            WG_ErrorAppend(error, error_size, "\n");
            goto failure;
        }
        if (!WG_ParseConfigLine(line, &tokens, &token_count,
                                &token_capacity, &line_start))
        {
            char number[32];
            (void)sprintf(number, "%lu", line_number);
            WG_ErrorAppend(error, error_size, "Malformed config line ");
            WG_ErrorAppend(error, error_size, number);
            WG_ErrorAppend(error, error_size, " in ");
            WG_ErrorAppend(error, error_size, selected_path);
            WG_ErrorAppend(error, error_size, "\n");
            goto failure;
        }
        line_end = token_count;
        if (line_end == line_start)
        {
            continue;
        }
        if (tokens[line_start][0] != '-')
        {
            WG_ErrorAppend(error, error_size,
                           "Each config line must begin with an option in ");
            WG_ErrorAppend(error, error_size, selected_path);
            WG_ErrorAppend(error, error_size, "\n");
            goto failure;
        }
        if (strcmp(tokens[line_start], "--config") == 0
            || strcmp(tokens[line_start], "--no-config") == 0)
        {
            WG_ErrorAppend(error, error_size,
                           "Config files cannot load or disable config files.\n");
            goto failure;
        }
        if (WG_CommandOverrides(argc, argv, tokens[line_start]))
        {
            while (token_count > line_start)
            {
                free(tokens[--token_count]);
            }
            continue;
        }
        if (WG_PathOption(tokens[line_start])
            && line_end == line_start + 2U
            && !WG_IsAbsolutePath(tokens[line_start + 1U]))
        {
            char *resolved = WG_JoinPath(config_directory,
                                         tokens[line_start + 1U]);
            if (resolved == NULL)
            {
                WG_ErrorAppend(error, error_size,
                               "Unable to resolve a config-file path.\n");
                goto failure;
            }
            free(tokens[line_start + 1U]);
            tokens[line_start + 1U] = resolved;
        }
    }
    if (ferror(file))
    {
        WG_ErrorAppend(error, error_size, "Unable to read config file: ");
        WG_ErrorAppend(error, error_size, selected_path);
        WG_ErrorAppend(error, error_size, "\n");
        goto failure;
    }
    (void)fclose(file);
    file = NULL;
    prepared->argv = (char **)malloc(
        ((size_t)argc + token_count + 1U) * sizeof(*prepared->argv));
    if (prepared->argv == NULL)
    {
        WG_ErrorAppend(error, error_size,
                       "Unable to allocate configured arguments.\n");
        goto failure;
    }
    prepared->argv[0] = argv[0];
    for (index = 0; index < (int)token_count; ++index)
    {
        prepared->argv[index + 1] = tokens[index];
    }
    for (index = 1; index < argc; ++index)
    {
        prepared->argv[(int)token_count + index] = argv[index];
    }
    prepared->argc = argc + (int)token_count;
    prepared->argv[prepared->argc] = NULL;
    prepared->config_argc = (int)token_count;
    prepared->config_path = selected_path;
    free(tokens);
    free(config_directory);
    return 1;

failure:
    if (file != NULL)
    {
        (void)fclose(file);
    }
    WG_FreeTokens(tokens, token_count);
    free(config_directory);
    free(selected_path);
    return 0;
}

void WG_FreeLauncherArguments(wg_launcher_arguments_t *prepared)
{
    int index;

    if (prepared == NULL)
    {
        return;
    }
    if (prepared->config_path != NULL)
    {
        for (index = 1; index <= prepared->config_argc; ++index)
        {
            free(prepared->argv[index]);
        }
        free(prepared->argv);
    }
    free(prepared->config_path);
    prepared->argc = 0;
    prepared->argv = NULL;
    prepared->config_argc = 0;
    prepared->config_path = NULL;
}

static int WG_ProfileFromText(const char *text, unsigned *profile)
{
    unsigned index;

    if (text == NULL || profile == NULL)
    {
        return 0;
    }
    if (*text == '.')
    {
        ++text;
    }
    for (index = 0U; index < WG_DISCOVERY_PROFILE_COUNT; ++index)
    {
        if (WG_ASCIIEqual(text, WG_DiscoveryProfiles[index].extension))
        {
            *profile = index;
            return 1;
        }
    }
    return 0;
}

static int WG_ExplicitProfile(int argc, char **argv,
                              unsigned *profile, int *present)
{
    int index;

    *present = 0;
    for (index = 1; index < argc; ++index)
    {
        unsigned candidate;

        if (strcmp(argv[index], "--game") == 0)
        {
            if (index + 1 >= argc
                || !WG_ProfileFromText(argv[index + 1], &candidate))
            {
                return 1;
            }
        }
        else if (argv[index][0] == '-' && argv[index][1] != '-'
                 && WG_ProfileFromText(argv[index] + 1, &candidate))
        {
        }
        else
        {
            continue;
        }
        if (*present && *profile != candidate)
        {
            return 0;
        }
        *profile = candidate;
        *present = 1;
    }
    return 1;
}

static int WG_HasDataArgument(int argc, char **argv)
{
    int index;

    for (index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--data") == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int WG_ExecutableHint(const char *path)
{
    const char *base = path;
    const char *cursor;
    const char *extension;
    size_t length;

    if (path == NULL)
    {
        return 0;
    }
    for (cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\')
        {
            base = cursor + 1;
        }
    }
    if (WG_ASCIIPrefix(base, "wolf"))
    {
        return 1;
    }
    extension = strrchr(base, '.');
    length = extension != NULL ? (size_t)(extension - base) : strlen(base);
    return length == 5U && WG_ASCIIPrefix(base, "spear") ? 2 : 0;
}

static size_t WG_ProfileCandidateCount(const wg_discovery_results_t *results,
                                       unsigned profile,
                                       const char **first_path)
{
    size_t index;
    size_t count = 0U;

    *first_path = NULL;
    for (index = 0U; index < results->count; ++index)
    {
        if (results->items[index].profile == profile)
        {
            if (count == 0U)
            {
                *first_path = results->items[index].path;
            }
            ++count;
        }
    }
    return count;
}

static void WG_ErrorAppend(char *error, size_t error_size, const char *text)
{
    size_t used;
    size_t available;
    size_t length;

    if (error == NULL || error_size == 0U || text == NULL)
    {
        return;
    }
    used = strlen(error);
    if (used >= error_size - 1U)
    {
        return;
    }
    available = error_size - used - 1U;
    length = strlen(text);
    if (length > available)
    {
        length = available;
    }
    memcpy(error + used, text, length);
    error[used + length] = '\0';
}

static void WG_AmbiguousError(char *error, size_t error_size,
                              const wg_discovery_results_t *results,
                              unsigned profile)
{
    size_t index;

    if (error != NULL && error_size != 0U)
    {
        error[0] = '\0';
    }
    WG_ErrorAppend(error, error_size, "Multiple valid ");
    WG_ErrorAppend(error, error_size,
                   WG_DiscoveryProfiles[profile].extension);
    WG_ErrorAppend(error, error_size,
                   " data directories were found; select one with --data PATH:\n");
    for (index = 0U; index < results->count; ++index)
    {
        if (results->items[index].profile == profile)
        {
            WG_ErrorAppend(error, error_size, "  ");
            WG_ErrorAppend(error, error_size, results->items[index].path);
            WG_ErrorAppend(error, error_size, "\n");
        }
    }
}

int WG_PrepareGameArguments(int argc, char **argv,
                            wg_game_arguments_t *prepared,
                            char *error, size_t error_size)
{
    static const unsigned wolf_order[] = { 0U, 1U, 2U, 3U };
    static const unsigned spear_order[] = { 2U, 3U, 0U, 1U };
    static const unsigned neutral_order[] = { 0U, 2U, 1U, 3U };
    const unsigned *order;
    size_t order_count = 4U;
    wg_discovery_results_t results;
    char *root;
    const char *selected_path = NULL;
    unsigned selected_profile = 0U;
    unsigned argument_profile = 0U;
    unsigned explicit_profile = 0U;
    int explicit_present;
    int hint;
    size_t index;
    size_t count = 0U;

    if (prepared == NULL || argc <= 0 || argv == NULL)
    {
        return 0;
    }
    prepared->argc = argc;
    prepared->argv = argv;
    prepared->allocated = 0;
    if (error != NULL && error_size != 0U)
    {
        error[0] = '\0';
    }
    if (WG_HasDataArgument(argc, argv))
    {
        return 1;
    }
    if (!WG_ExplicitProfile(argc, argv, &explicit_profile,
                            &explicit_present))
    {
        WG_ErrorAppend(error, error_size,
                       "Conflicting game-data selectors were supplied.\n");
        return 0;
    }
    root = WG_ExecutableDirectory(argc > 0 ? argv[0] : NULL);
    if (root == NULL)
    {
        WG_ErrorAppend(error, error_size,
                       "Unable to allocate the game-data search path.\n");
        return 0;
    }
    memset(&results, 0, sizeof(results));
    WG_ScanDirectory(root, &results);
    free(root);
    if (results.failed)
    {
        WG_FreeDiscoveryResults(&results);
        WG_ErrorAppend(error, error_size,
                       "Unable to complete the game-data directory scan.\n");
        return 0;
    }
    if (explicit_present)
    {
        selected_profile = explicit_profile;
        argument_profile = explicit_profile;
        count = WG_ProfileCandidateCount(&results, selected_profile,
                                         &selected_path);
        if (count == 0U && selected_profile >= 4U)
        {
            selected_profile = 2U;
            count = WG_ProfileCandidateCount(&results, selected_profile,
                                             &selected_path);
        }
    }
    else
    {
        hint = WG_ExecutableHint(argc > 0 ? argv[0] : NULL);
        order = hint == 1 ? wolf_order
              : hint == 2 ? spear_order : neutral_order;
        for (index = 0U; index < order_count; ++index)
        {
            count = WG_ProfileCandidateCount(&results, order[index],
                                             &selected_path);
            if (count != 0U)
            {
                selected_profile = order[index];
                argument_profile = selected_profile;
                break;
            }
        }
    }
    if (count > 1U)
    {
        WG_AmbiguousError(error, error_size, &results,
                          selected_profile);
        WG_FreeDiscoveryResults(&results);
        return 0;
    }
    if (count == 1U)
    {
        char *path_copy = WG_CopyString(selected_path);
        char **arguments = (char **)malloc(
            ((size_t)argc + 5U) * sizeof(*arguments));

        if (path_copy == NULL || arguments == NULL)
        {
            free(path_copy);
            free(arguments);
            WG_FreeDiscoveryResults(&results);
            WG_ErrorAppend(error, error_size,
                           "Unable to allocate discovered game-data arguments.\n");
            return 0;
        }
        for (index = 0U; index < (size_t)argc; ++index)
        {
            arguments[index] = argv[index];
        }
        arguments[argc] = (char *)"--data";
        arguments[argc + 1] = path_copy;
        arguments[argc + 2] = (char *)"--game";
        arguments[argc + 3] = (char *)WG_DiscoveryProfiles[
            argument_profile].extension;
        arguments[argc + 4] = NULL;
        prepared->argc = argc + 4;
        prepared->argv = arguments;
        prepared->allocated = 1;
    }
    WG_FreeDiscoveryResults(&results);
    return 1;
}

void WG_FreeGameArguments(wg_game_arguments_t *prepared)
{
    if (prepared != NULL && prepared->allocated)
    {
        free(prepared->argv[prepared->argc - 3]);
        free(prepared->argv);
        prepared->argv = NULL;
        prepared->argc = 0;
        prepared->allocated = 0;
    }
}

void WG_PrintDiagnostics(int argc, char **argv,
                         const char *config_path,
                         const char *host_report)
{
    wg_help_stream_t stream = WG_OpenHelpStream();
    wg_discovery_results_t results;
    wg_game_arguments_t selected;
    char error[2048];
    char number[32];
    char *root;
    size_t index;

    if (!WG_HelpStreamValid(&stream))
    {
        return;
    }
    WG_HelpWrite(&stream, "wolf3d portable diagnostics\n\nExecutable: ");
    WG_HelpWrite(&stream, argc > 0 ? argv[0] : "(unknown)");
    WG_HelpWrite(&stream, "\nConfig:     ");
    WG_HelpWrite(&stream, config_path != NULL ? config_path : "(none)");
    WG_HelpWrite(&stream, "\nArguments:");
    for (index = 1U; index < (size_t)argc; ++index)
    {
        WG_HelpWrite(&stream, " ");
        WG_HelpWrite(&stream, argv[index]);
    }
    WG_HelpWrite(&stream, "\n\nGame data scan\n");
    root = WG_ExecutableDirectory(argc > 0 ? argv[0] : NULL);
    if (root == NULL)
    {
        WG_HelpWrite(&stream, "  Unable to allocate scan path.\n");
    }
    else
    {
        WG_HelpWrite(&stream, "  Root: ");
        WG_HelpWrite(&stream, root);
        WG_HelpWrite(&stream, "\n");
        memset(&results, 0, sizeof(results));
        WG_ScanDirectory(root, &results);
        if (results.failed)
        {
            WG_HelpWrite(&stream, "  Scan failed.\n");
        }
        else if (results.count == 0U)
        {
            WG_HelpWrite(&stream, "  No complete data sets found.\n");
        }
        else
        {
            for (index = 0U; index < results.count; ++index)
            {
                WG_HelpWrite(&stream, "  ");
                WG_HelpWrite(&stream,
                    WG_DiscoveryProfiles[results.items[index].profile].extension);
                WG_HelpWrite(&stream, ": ");
                WG_HelpWrite(&stream, results.items[index].path);
                WG_HelpWrite(&stream, "\n");
            }
        }
        WG_FreeDiscoveryResults(&results);
        free(root);
    }
    if (WG_PrepareGameArguments(argc, argv, &selected,
                                error, sizeof(error)))
    {
        WG_HelpWrite(&stream, "  Selection: ");
        if (selected.allocated)
        {
            WG_HelpWrite(&stream, selected.argv[selected.argc - 1]);
            WG_HelpWrite(&stream, " at ");
            WG_HelpWrite(&stream, selected.argv[selected.argc - 3]);
        }
        else
        {
            WG_HelpWrite(&stream,
                "explicit --data selection or no automatic match");
        }
        WG_HelpWrite(&stream, "\n");
        WG_FreeGameArguments(&selected);
    }
    else
    {
        WG_HelpWrite(&stream, "  Selection error: ");
        WG_HelpWrite(&stream, error);
    }
    WG_HelpWrite(&stream, "\nHost hardware\n");
    WG_HelpWrite(&stream, host_report != NULL ? host_report
                                               : "  No host report available.\n");
    (void)sprintf(number, "%lu", (unsigned long)wolf3d_GetOPLDriverCount());
    WG_HelpWrite(&stream, "\nAudio emulation\n  Compiled OPL drivers: ");
    WG_HelpWrite(&stream, number);
    WG_HelpWrite(&stream, " (");
    for (index = 0U; index < wolf3d_GetOPLDriverCount(); ++index)
    {
        if (index != 0U)
        {
            WG_HelpWrite(&stream, ", ");
        }
        WG_HelpWrite(&stream, wolf3d_GetOPLDriverName(index));
    }
    WG_HelpWrite(&stream, ")\n");
    WG_CloseHelpStream(&stream);
}

void WG_PrintLauncherError(const char *message)
{
    wg_help_stream_t stream = WG_OpenHelpStream();

    if (WG_HelpStreamValid(&stream))
    {
        WG_HelpWrite(&stream, "wolf3d: ");
        WG_HelpWrite(&stream, message != NULL ? message : "launcher error\n");
    }
    WG_CloseHelpStream(&stream);
}
