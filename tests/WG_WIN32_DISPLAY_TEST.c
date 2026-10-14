#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
typedef long LONG;
typedef struct { unsigned dmSize, dmFields, dmPelsWidth, dmPelsHeight, dmBitsPerPel, dmDisplayFrequency; } DEVMODEA;
#define ZeroMemory(p,n) memset(p,0,n)
#define ENUM_CURRENT_SETTINGS (~0U)
#define DM_PELSWIDTH 1
#define DM_PELSHEIGHT 2
#define DM_BITSPERPEL 4
#define CDS_TEST 2
#define CDS_FULLSCREEN 4
#define DISP_CHANGE_SUCCESSFUL 0
static int EnumDisplaySettingsA(const char *, unsigned, DEVMODEA *);
static LONG ChangeDisplaySettingsA(DEVMODEA *, unsigned);
#include "../platforms/win32/WG_WIN32_DISPLAY.h"
static wg_display_mode_t state;
static unsigned queries, changes, fail_flags = 999;
static int fail_query;
static int EnumDisplaySettingsA(const char *device, unsigned setting, DEVMODEA *mode)
{
    assert(device == NULL && setting == ENUM_CURRENT_SETTINGS);
    assert(mode->dmSize == sizeof(*mode));
    ++queries;
    mode->dmPelsWidth = 1280; mode->dmPelsHeight = 1024;
    mode->dmBitsPerPel = 16; mode->dmDisplayFrequency = 75;
    return !fail_query;
}
static LONG ChangeDisplaySettingsA(DEVMODEA *mode, unsigned flags)
{
    assert(state.changing);
    ++changes;
    if (flags == 0)
    {
        assert(mode->dmPelsWidth == 1280 && mode->dmPelsHeight == 1024);
        assert(mode->dmDisplayFrequency == 75 && mode->dmBitsPerPel == 16);
    }
    else
    {
        assert(mode->dmPelsWidth == 640 && mode->dmPelsHeight == 480);
        assert(mode->dmFields == (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL));
        assert(mode->dmBitsPerPel == 16);
    }
    return flags == fail_flags ? -2 : 0;
}
int main(void)
{
    unsigned scale = 3, w = 0, h = 0, before;
    assert(WG_ParseWindowScale("1", &scale) && scale == 1);
    assert(WG_ParseWindowScale("8", &scale) && scale == 8);
    assert(!WG_ParseWindowScale("0", &scale));
    assert(!WG_ParseWindowScale("9", &scale));
    assert(!WG_ParseWindowScale("2x", &scale));
    assert(!WG_ParseWindowScale("-1", &scale));
    assert(!WG_ParseWindowScale("99999999999999", &scale));
    assert(WG_ParseFullscreenResolution("640x480", &w, &h) && w == 640 && h == 480);
    assert(WG_ParseFullscreenResolution("1280X1024", &w, &h));
    assert(!WG_ParseFullscreenResolution("640x480junk", &w, &h));
    assert(!WG_ParseFullscreenResolution("640", &w, &h));
    assert(!WG_ParseFullscreenResolution("0x480", &w, &h));
    assert(!WG_ParseFullscreenResolution("640x999999999", &w, &h));
    assert(WG_DisplayEnable(&state) && queries == 0 && changes == 0);
    assert(WG_DisplayRestore(&state, 1));
    state.width = 640; state.height = 480;
    fail_query = 1;
    assert(!WG_DisplayEnable(&state) && !state.saved && changes == 0);
    fail_query = 0; fail_flags = CDS_TEST;
    assert(!WG_DisplayEnable(&state) && !state.active && !state.changing);
    assert(WG_DisplayRestore(&state, 1) && !state.saved);
    fail_flags = CDS_FULLSCREEN;
    assert(!WG_DisplayEnable(&state) && !state.active);
    assert(WG_DisplayRestore(&state, 1));
    fail_flags = 999;
    assert(WG_DisplayEnable(&state) && state.active && state.saved);
    before = queries;
    assert(WG_DisplayRestore(&state, 0) && !state.active && state.saved);
    assert(WG_DisplayEnable(&state) && state.active && queries == before);
    fail_flags = 0;
    assert(!WG_DisplayRestore(&state, 1) && state.active && state.saved);
    fail_flags = 999;
    assert(WG_DisplayRestore(&state, 1) && !state.active && !state.saved);
    before = changes;
    assert(WG_DisplayRestore(&state, 1) && changes == before);
    puts("Display parsing and mode lifecycle tests passed");
    return 0;
}
