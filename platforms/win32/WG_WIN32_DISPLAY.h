#ifndef WG_WIN32_DISPLAY_H
#define WG_WIN32_DISPLAY_H

/* Windows 95-compatible primary-display mode management. */
typedef struct wg_display_mode
{
    DEVMODEA desktop;
    unsigned width, height;
    int saved, active, changing;
} wg_display_mode_t;

static int WG_DisplayNumber(const char **cursor, unsigned *value)
{
    unsigned n = 0;
    const char *p = *cursor;
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9')
    {
        n = n * 10U + (unsigned)(*p++ - '0');
        if (n > 8192U) return 0;
    }
    *cursor = p;
    *value = n;
    return 1;
}

static int WG_ParseWindowScale(const char *text, unsigned *scale)
{
    unsigned n;
    if (!WG_DisplayNumber(&text, &n) || *text != 0 || n < 1 || n > 8) return 0;
    *scale = n;
    return 1;
}

static int WG_ParseFullscreenResolution(const char *text, unsigned *width,
                                        unsigned *height)
{
    unsigned w, h;
    if (!WG_DisplayNumber(&text, &w) || (*text != 'x' && *text != 'X')) return 0;
    ++text;
    if (!WG_DisplayNumber(&text, &h) || *text != 0 || w < 320 || h < 240) return 0;
    *width = w;
    *height = h;
    return 1;
}

static int WG_DisplayEnable(wg_display_mode_t *state)
{
    DEVMODEA requested;
    LONG result;
    if (state->width == 0 || state->active) return 1;
    if (!state->saved)
    {
        ZeroMemory(&state->desktop, sizeof(state->desktop));
        state->desktop.dmSize = sizeof(state->desktop);
        if (!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &state->desktop)) return 0;
        state->saved = 1;
    }
    ZeroMemory(&requested, sizeof(requested));
    requested.dmSize = sizeof(requested);
    requested.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    requested.dmPelsWidth = state->width;
    requested.dmPelsHeight = state->height;
    requested.dmBitsPerPel = state->desktop.dmBitsPerPel;
    state->changing = 1;
    result = ChangeDisplaySettingsA(&requested, CDS_TEST);
    if (result == DISP_CHANGE_SUCCESSFUL)
        result = ChangeDisplaySettingsA(&requested, CDS_FULLSCREEN);
    state->active = result == DISP_CHANGE_SUCCESSFUL;
    state->changing = 0;
    return state->active;
}

static int WG_DisplayRestore(wg_display_mode_t *state, int forget)
{
    LONG result = DISP_CHANGE_SUCCESSFUL;
    if (state->active)
    {
        state->changing = 1;
        result = ChangeDisplaySettingsA(&state->desktop, 0);
        state->changing = 0;
        if (result == DISP_CHANGE_SUCCESSFUL) state->active = 0;
    }
    if (forget && !state->active) state->saved = 0;
    return result == DISP_CHANGE_SUCCESSFUL;
}
#endif
