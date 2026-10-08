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
