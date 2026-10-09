#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "WOLF3D.h"
#include "../WG_HELP.h"
#include "../WG_HOST.h"
#include "../WG_TEXT_OUTPUT.h"
#ifdef WG_WIN9X
#include "../dos/WG_DOS_ADLIB.h"
#endif

#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#if defined(_MSC_VER) && _MSC_VER < 1600
#include "WOLF3D_STDINT.h"
#else
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const TCHAR wg_window_class[] = TEXT("wolf3d-window");
static HWND wg_window;
static HCURSOR wg_blank_cursor;
static uint32_t wg_pixels[WOLF3D_SCREEN_WIDTH * WOLF3D_SCREEN_HEIGHT];
static LARGE_INTEGER wg_counter_frequency;
static LARGE_INTEGER wg_counter_start;
static int wg_quit_pending;
static int wg_start_fullscreen;
static int wg_fullscreen;
static int wg_fullscreen_enter_down;
static int wg_mouse_mode = -1;
static int wg_mouse_enabled;
static int wg_mouse_captured;
static int wg_mouse_capture_requested;
static int wg_joystick_mode = -1;
#ifndef WG_LEGACY_WIN32
static LONG wg_raw_mouse_x;
static LONG wg_raw_mouse_y;
static int wg_raw_mouse_position_valid;
static int wg_raw_mouse_wobble;
#endif
#ifdef WG_LEGACY_WIN32
static LONG wg_windowed_style;
#else
static LONG_PTR wg_windowed_style;
#endif
static WINDOWPLACEMENT wg_windowed_placement = { 0 };
static uint8_t wg_text_screen[WOLF3D_TEXT_COLUMNS * WOLF3D_TEXT_ROWS
                              * WOLF3D_TEXT_CELL_BYTES];
static uint16_t wg_text_columns;
static uint16_t wg_text_rows;
static HANDLE wg_console_output;
#ifdef WG_LEGACY_WIN32
static POINT wg_legacy_mouse_position;
static int wg_legacy_mouse_position_valid;
#endif

/* XInput is loaded at run time, so carry its small public ABI here instead of
   requiring a particular Windows SDK.  This keeps the same joystick path
   available to the XP-era compiler bands. */
typedef struct wg_xinput_gamepad
{
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} wg_xinput_gamepad_t;

typedef struct wg_xinput_state
{
    DWORD dwPacketNumber;
    wg_xinput_gamepad_t Gamepad;
} wg_xinput_state_t;

#define WG_XINPUT_GAMEPAD_DPAD_UP    0x0001U
#define WG_XINPUT_GAMEPAD_DPAD_DOWN  0x0002U
#define WG_XINPUT_GAMEPAD_DPAD_LEFT  0x0004U
#define WG_XINPUT_GAMEPAD_DPAD_RIGHT 0x0008U
#define WG_XINPUT_GAMEPAD_A          0x1000U
#define WG_XINPUT_GAMEPAD_B          0x2000U
#define WG_XINPUT_GAMEPAD_X          0x4000U
#define WG_XINPUT_GAMEPAD_Y          0x8000U

typedef DWORD (WINAPI *wg_xinput_get_state_t)(DWORD, wg_xinput_state_t *);
static HMODULE wg_xinput_module;
static wg_xinput_get_state_t wg_xinput_get_state;
static int16_t wg_joystick_x[WOLF3D_MAX_JOYSTICKS];
static int16_t wg_joystick_y[WOLF3D_MAX_JOYSTICKS];
static uint32_t wg_joystick_buttons[WOLF3D_MAX_JOYSTICKS];
static uint8_t wg_joystick_connected[WOLF3D_MAX_JOYSTICKS];

#define WG_PCM_BUFFER_COUNT 4U
#define WG_PCM_BUFFER_FRAMES 512U
#define WG_DISPLAY_ASPECT_WIDTH 4
#define WG_DISPLAY_ASPECT_HEIGHT 3
#define WG_INITIAL_SCALE 3
static HWAVEOUT wg_wave_out;
static WAVEHDR wg_wave_headers[WG_PCM_BUFFER_COUNT];
static int16_t wg_wave_samples[WG_PCM_BUFFER_COUNT]
                              [WG_PCM_BUFFER_FRAMES * 2U];
static uint8_t wg_wave_used[WG_PCM_BUFFER_COUNT];

static void WG_Win32PCMShutdown(void);

static int wg_attach_parent_console(void)
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

static HANDLE wg_console_output_handle(void)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;

    if (wg_console_output != NULL
        && wg_console_output != INVALID_HANDLE_VALUE)
    {
        return wg_console_output;
    }
    if (output != NULL && output != INVALID_HANDLE_VALUE
        && GetConsoleMode(output, &mode))
    {
        wg_console_output = output;
        return output;
    }
    if ((output == NULL || output == INVALID_HANDLE_VALUE)
        && wg_attach_parent_console())
    {
        output = CreateFile(TEXT("CONOUT$"), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, 0, NULL);
        if (output != INVALID_HANDLE_VALUE)
        {
            wg_console_output = output;
            return output;
        }
    }
    return NULL;
}

static void wg_write_text_screen(void)
{
    if (wg_text_columns == 0U || wg_text_rows == 0U)
    {
        return;
    }
    WG_WriteWindowsTextScreen(wg_text_screen, wg_text_columns, wg_text_rows);
    wg_text_columns = 0U;
    wg_text_rows = 0U;
}

static RECT wg_presentation_rectangle(const RECT *client)
{
    RECT presentation = { 0, 0, 0, 0 };
    LONG client_width;
    LONG client_height;
    LONG width;
    LONG height;

    if (client == NULL)
    {
        return presentation;
    }
    client_width = client->right - client->left;
    client_height = client->bottom - client->top;
    if (client_width <= 0 || client_height <= 0)
    {
        return presentation;
    }

    if ((int64_t)client_width * WG_DISPLAY_ASPECT_HEIGHT
        > (int64_t)client_height * WG_DISPLAY_ASPECT_WIDTH)
    {
        height = client_height;
        width = (height * WG_DISPLAY_ASPECT_WIDTH)
              / WG_DISPLAY_ASPECT_HEIGHT;
    }
    else
    {
        width = client_width;
        height = (width * WG_DISPLAY_ASPECT_HEIGHT)
               / WG_DISPLAY_ASPECT_WIDTH;
    }
    presentation.left = client->left + (client_width - width) / 2;
    presentation.top = client->top + (client_height - height) / 2;
    presentation.right = presentation.left + width;
    presentation.bottom = presentation.top + height;
    return presentation;
}

#define WOLF3D_EVENT_QUEUE_CAPACITY 64U
static wolf3d_event_t wg_event_queue[WOLF3D_EVENT_QUEUE_CAPACITY];
static size_t wg_event_read;
static size_t wg_event_write;

static void wg_queue_event(const wolf3d_event_t *event)
{
    size_t next = (wg_event_write + 1U) % WOLF3D_EVENT_QUEUE_CAPACITY;

    if (next != wg_event_read)
    {
        wg_event_queue[wg_event_write] = *event;
        wg_event_write = next;
    }
}

static void wg_queue_mouse_button(uint8_t button, int pressed)
{
    wolf3d_event_t event = { 0 };

    event.type = WOLF3D_EVENT_MOUSE_BUTTON;
    event.pressed = pressed;
    event.key = 0;
    event.x = 0;
    event.y = 0;
    event.button = button;
    wg_queue_event(&event);
}

static void wg_load_xinput(void)
{
    static const TCHAR *const libraries[] =
    {
        TEXT("xinput1_4.dll"), TEXT("xinput1_3.dll"),
        TEXT("xinput9_1_0.dll")
    };
    size_t index;

    for (index = 0U; index < sizeof(libraries) / sizeof(libraries[0]); ++index)
    {
        wg_xinput_module = LoadLibrary(libraries[index]);
        if (wg_xinput_module != NULL)
        {
            FARPROC procedure = GetProcAddress(wg_xinput_module,
                                               "XInputGetState");

            memcpy(&wg_xinput_get_state, &procedure,
                   sizeof(wg_xinput_get_state));
            if (wg_xinput_get_state != NULL)
            {
                return;
            }
            FreeLibrary(wg_xinput_module);
            wg_xinput_module = NULL;
        }
    }
}

static void wg_poll_joysticks(void)
{
    DWORD joystick;

    if (wg_joystick_mode == 0 || wg_xinput_get_state == NULL)
    {
        return;
    }
    for (joystick = 0U; joystick < WOLF3D_MAX_JOYSTICKS; ++joystick)
    {
        wg_xinput_state_t state;
        int connected;
        int16_t x = 0;
        int16_t y = 0;
        uint32_t buttons = 0U;

        ZeroMemory(&state, sizeof(state));
        connected = wg_xinput_get_state(joystick, &state) == ERROR_SUCCESS;
        if (connected)
        {
            WORD source = state.Gamepad.wButtons;
            SHORT source_y = state.Gamepad.sThumbLY;

            x = state.Gamepad.sThumbLX;
            y = source_y == INT16_MIN ? INT16_MAX : (int16_t)-source_y;
            if ((source & WG_XINPUT_GAMEPAD_DPAD_LEFT) != 0U)
            {
                x = INT16_MIN;
            }
            else if ((source & WG_XINPUT_GAMEPAD_DPAD_RIGHT) != 0U)
            {
                x = INT16_MAX;
            }
            if ((source & WG_XINPUT_GAMEPAD_DPAD_UP) != 0U)
            {
                y = INT16_MIN;
            }
            else if ((source & WG_XINPUT_GAMEPAD_DPAD_DOWN) != 0U)
            {
                y = INT16_MAX;
            }
            buttons |= (source & WG_XINPUT_GAMEPAD_A) != 0U ? 1U : 0U;
            buttons |= (source & WG_XINPUT_GAMEPAD_B) != 0U ? 2U : 0U;
            buttons |= (source & WG_XINPUT_GAMEPAD_X) != 0U ? 4U : 0U;
            buttons |= (source & WG_XINPUT_GAMEPAD_Y) != 0U ? 8U : 0U;
        }
        if (connected != wg_joystick_connected[joystick]
            || x != wg_joystick_x[joystick]
            || y != wg_joystick_y[joystick]
            || buttons != wg_joystick_buttons[joystick])
        {
            wolf3d_event_t event = { 0 };

            wg_joystick_connected[joystick] = (uint8_t)connected;
            wg_joystick_x[joystick] = x;
            wg_joystick_y[joystick] = y;
            wg_joystick_buttons[joystick] = buttons;
            event.type = WOLF3D_EVENT_JOYSTICK;
            event.joystick = (uint8_t)joystick;
            event.connected = (uint8_t)connected;
            event.x = x;
            event.y = y;
            event.buttons = buttons;
            wg_queue_event(&event);
        }
    }
}

static void WG_Win32UpdateMouseClip(void)
{
    RECT rectangle;
    POINT upper_left;
    POINT lower_right;

    if (!wg_mouse_captured || wg_window == NULL
        || !GetClientRect(wg_window, &rectangle))
    {
        return;
    }
    upper_left.x = rectangle.left;
    upper_left.y = rectangle.top;
    lower_right.x = rectangle.right;
    lower_right.y = rectangle.bottom;
    if (ClientToScreen(wg_window, &upper_left)
        && ClientToScreen(wg_window, &lower_right))
    {
        rectangle.left = upper_left.x;
        rectangle.top = upper_left.y;
        rectangle.right = lower_right.x;
        rectangle.bottom = lower_right.y;
        (void)ClipCursor(&rectangle);
    }
}

static int WG_Win32GetMouseCenter(POINT *screen_center)
{
    RECT client;

    if (wg_window == NULL || !GetClientRect(wg_window, &client))
    {
        return 0;
    }
    screen_center->x = (client.right - client.left) / 2;
    screen_center->y = (client.bottom - client.top) / 2;
    if (!ClientToScreen(wg_window, screen_center))
    {
        return 0;
    }
    return 1;
}

#ifndef WG_LEGACY_WIN32
static void WG_Win32WarpMouseCursor(int x, int y)
{
    /* RDP caches and coalesces cursor warps.  The one-pixel intermediate
       position makes a repeated center warp observable to the remote host. */
    (void)SetCursorPos(x, y);
    (void)SetCursorPos(x + 1, y);
    (void)SetCursorPos(x, y);
}
#endif

static void WG_Win32ApplyMouseCapture(int capture)
{
    capture = capture && wg_mouse_enabled && wg_window != NULL;
    if (capture == wg_mouse_captured)
    {
        if (capture) WG_Win32UpdateMouseClip();
        return;
    }
    wg_mouse_captured = capture;
    if (capture)
    {
        POINT screen_center;

        SetCapture(wg_window);
        WG_Win32UpdateMouseClip();
        SetCursor(wg_blank_cursor);
        if (WG_Win32GetMouseCenter(&screen_center))
        {
#ifdef WG_LEGACY_WIN32
            (void)SetCursorPos(screen_center.x, screen_center.y);
#else
            WG_Win32WarpMouseCursor(screen_center.x, screen_center.y);
#endif
        }
    }
    else
    {
        ClipCursor(NULL);
        if (GetCapture() == wg_window) ReleaseCapture();
        SetCursor(wg_fullscreen ? wg_blank_cursor
                                : LoadCursor(NULL, IDC_ARROW));
    }
#ifndef WG_LEGACY_WIN32
    wg_raw_mouse_position_valid = 0;
    wg_raw_mouse_wobble = 0;
#endif
}

static void WG_Win32RequestMouseCapture(int capture)
{
    wg_mouse_capture_requested = capture != 0;
    WG_Win32ApplyMouseCapture(wg_mouse_capture_requested
                              && GetForegroundWindow() == wg_window);
}

static int WG_Win32SetFullscreen(int fullscreen)
{
    if (wg_window == NULL || fullscreen == wg_fullscreen)
    {
        return 1;
    }
    if (fullscreen)
    {
#ifdef WG_LEGACY_WIN32
        wg_windowed_style = GetWindowLong(wg_window, GWL_STYLE);
        wg_windowed_placement.length = sizeof(wg_windowed_placement);
        if (!GetWindowPlacement(wg_window, &wg_windowed_placement))
        {
            return 0;
        }
        SetWindowLong(wg_window, GWL_STYLE,
                       wg_windowed_style & ~(LONG)WS_OVERLAPPEDWINDOW);
        if (!SetWindowPos(wg_window, HWND_TOP, 0, 0,
                          GetSystemMetrics(SM_CXSCREEN),
                          GetSystemMetrics(SM_CYSCREEN),
                          SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
        {
            SetWindowLong(wg_window, GWL_STYLE, wg_windowed_style);
            return 0;
        }
#else
        MONITORINFO monitor = { 0 };

        wg_windowed_style = GetWindowLongPtrW(wg_window, GWL_STYLE);
        wg_windowed_placement.length = sizeof(wg_windowed_placement);
        monitor.cbSize = sizeof(monitor);
        if (!GetWindowPlacement(wg_window, &wg_windowed_placement)
            || !GetMonitorInfoW(MonitorFromWindow(
                                    wg_window, MONITOR_DEFAULTTONEAREST),
                                &monitor))
        {
            return 0;
        }
        SetWindowLongPtrW(wg_window, GWL_STYLE,
                          wg_windowed_style
                              & ~(LONG_PTR)WS_OVERLAPPEDWINDOW);
        if (!SetWindowPos(wg_window, HWND_TOP,
                          monitor.rcMonitor.left, monitor.rcMonitor.top,
                          monitor.rcMonitor.right - monitor.rcMonitor.left,
                          monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                          SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
        {
            SetWindowLongPtrW(wg_window, GWL_STYLE, wg_windowed_style);
            return 0;
        }
#endif
    }
    else
    {
#ifdef WG_LEGACY_WIN32
        SetWindowLong(wg_window, GWL_STYLE, wg_windowed_style);
#else
        SetWindowLongPtrW(wg_window, GWL_STYLE, wg_windowed_style);
#endif
        if (!SetWindowPlacement(wg_window, &wg_windowed_placement)
            || !SetWindowPos(wg_window, NULL, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                                 | SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
        {
            return 0;
        }
    }
    wg_fullscreen = fullscreen;
    SetCursor((fullscreen || wg_mouse_captured)
                  ? wg_blank_cursor : LoadCursor(NULL, IDC_ARROW));
    WG_Win32UpdateMouseClip();
    return 1;
}

static LRESULT CALLBACK wg_window_proc(HWND window, UINT message,
                                       WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_SETCURSOR:
            if ((wg_fullscreen || wg_mouse_captured)
                && LOWORD(lparam) == HTCLIENT)
            {
                /* RDP requires a real cursor object to continue transmitting
                   motion and to honor SetCursorPos.  Use a transparent cursor
                   rather than a NULL handle or the ShowCursor counter. */
                SetCursor(wg_blank_cursor);
                return TRUE;
            }
            return DefWindowProc(window, message, wparam, lparam);

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            wolf3d_event_t event = { 0 };
            int pressed = message == WM_KEYDOWN
                       || message == WM_SYSKEYDOWN;
            int alt_enter = wparam == VK_RETURN
                         && (wg_fullscreen_enter_down
                             || (pressed
                                 && (GetKeyState(VK_MENU) & 0x8000) != 0));

            /* F11 belongs entirely to the presentation host.  Consume both
               transitions so it never satisfies an engine "any key" wait. */
            if (wparam == VK_F11)
            {
                if (pressed && (lparam & ((LPARAM)1U << 30)) == 0)
                {
                    (void)WG_Win32SetFullscreen(!wg_fullscreen);
                }
                return 0;
            }

            if (alt_enter)
            {
                if (pressed && !wg_fullscreen_enter_down
                    && (lparam & ((LPARAM)1U << 30)) == 0)
                {
                    (void)WG_Win32SetFullscreen(!wg_fullscreen);
                    wg_fullscreen_enter_down = 1;
                }
                else if (!pressed)
                {
                    wg_fullscreen_enter_down = 0;
                }
                return 0;
            }

            event.type = WOLF3D_EVENT_KEY;
            event.pressed = pressed;
            event.key = wparam == VK_PAUSE
                            ? WOLF3D_KEY_PAUSE
                            : (uint16_t)((lparam >> 16) & 0xff);
            event.x = 0;
            event.y = 0;
            event.button = 0;
            wg_queue_event(&event);
            return 0;
        }

#ifndef WG_LEGACY_WIN32
        case WM_INPUT:
        {
            RAWINPUT input;
            UINT size = sizeof(input);

            if (wg_mouse_enabled && wg_mouse_captured
                && GetRawInputData((HRAWINPUT)lparam, RID_INPUT, &input, &size,
                                sizeof(RAWINPUTHEADER)) == sizeof(input)
                && input.header.dwType == RIM_TYPEMOUSE)
            {
                LONG x = input.data.mouse.lLastX;
                LONG y = input.data.mouse.lLastY;

                if ((input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0U)
                {
                    const USHORT raw_absolute_coordinates = 0x40U;
                    int virtual_desktop = (input.data.mouse.usFlags
                                           & MOUSE_VIRTUAL_DESKTOP) != 0U;
                    int screen_width = GetSystemMetrics(
                        virtual_desktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
                    int screen_height = GetSystemMetrics(
                        virtual_desktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
                    LONG absolute_x;
                    LONG absolute_y;
                    LONG relative_x = 0;
                    LONG relative_y = 0;
                    int at_edge;

                    if ((input.data.mouse.usFlags
                         & raw_absolute_coordinates) != 0U)
                    {
                        absolute_x = x;
                        absolute_y = y;
                    }
                    else
                    {
                        absolute_x = MulDiv(x, screen_width, 65535);
                        absolute_y = MulDiv(y, screen_height, 65535);
                    }
                    if (wg_raw_mouse_position_valid)
                    {
                        relative_x = absolute_x - wg_raw_mouse_x;
                        relative_y = absolute_y - wg_raw_mouse_y;
                    }
                    wg_raw_mouse_x = absolute_x;
                    wg_raw_mouse_y = absolute_y;
                    wg_raw_mouse_position_valid = 1;

                    at_edge = absolute_x <= screen_width / 100
                           || absolute_x >= screen_width - screen_width / 100
                           || absolute_y <= screen_height / 100
                           || absolute_y >= screen_height
                                               - screen_height / 100
                           || absolute_y < 32;
                    if (at_edge)
                    {
                        POINT center;

                        if (WG_Win32GetMouseCenter(&center))
                        {
                            WG_Win32WarpMouseCursor(
                                center.x + wg_raw_mouse_wobble, center.y);
                            ++wg_raw_mouse_wobble;
                            if (wg_raw_mouse_wobble > 1)
                            {
                                wg_raw_mouse_wobble = -1;
                            }
                        }
                        x = 0;
                        y = 0;
                    }
                    else if (relative_x > -screen_height / 6
                             && relative_x < screen_height / 6
                             && relative_y > -screen_height / 6
                             && relative_y < screen_height / 6)
                    {
                        x = relative_x;
                        y = relative_y;
                    }
                    else
                    {
                        x = 0;
                        y = 0;
                    }
                }

                if (x != 0 || y != 0)
                {
                    wolf3d_event_t event = { 0 };

                    event.type = WOLF3D_EVENT_MOUSE_MOTION;
                    event.pressed = 0;
                    event.key = 0;
                    event.x = (int16_t)(x > INT16_MAX ? INT16_MAX
                                         : x < INT16_MIN ? INT16_MIN : x);
                    event.y = (int16_t)(y > INT16_MAX ? INT16_MAX
                                         : y < INT16_MIN ? INT16_MIN : y);
                    event.button = 0;
                    wg_queue_event(&event);
                }
            }
            return 0;
        }
#endif

#ifdef WG_LEGACY_WIN32
        case WM_MOUSEMOVE:
        {
            POINT position;

            position.x = (SHORT)LOWORD(lparam);
            position.y = (SHORT)HIWORD(lparam);
            if (wg_mouse_enabled && wg_mouse_captured)
            {
                RECT client;
                POINT center;
                LONG x;
                LONG y;

                GetClientRect(window, &client);
                center.x = (client.right - client.left) / 2;
                center.y = (client.bottom - client.top) / 2;
                x = position.x - center.x;
                y = position.y - center.y;
                if (x != 0 || y != 0)
                {
                    wolf3d_event_t event = { 0 };
                    POINT screen_center = center;

                    event.type = WOLF3D_EVENT_MOUSE_MOTION;
                    event.x = (int16_t)(x > INT16_MAX ? INT16_MAX
                                         : x < INT16_MIN ? INT16_MIN : x);
                    event.y = (int16_t)(y > INT16_MAX ? INT16_MAX
                                         : y < INT16_MIN ? INT16_MIN : y);
                    wg_queue_event(&event);
                    if (ClientToScreen(window, &screen_center))
                    {
                        SetCursorPos(screen_center.x, screen_center.y);
                    }
                }
            }
            wg_legacy_mouse_position = position;
            wg_legacy_mouse_position_valid = 1;
            return 0;
        }

#endif

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
            int pressed = message == WM_LBUTTONDOWN
                       || message == WM_RBUTTONDOWN
                       || message == WM_MBUTTONDOWN;
            uint8_t button = (message == WM_LBUTTONDOWN
                              || message == WM_LBUTTONUP) ? 1U
                           : (message == WM_RBUTTONDOWN
                              || message == WM_RBUTTONUP) ? 2U : 3U;

            if (!wg_mouse_enabled || !wg_mouse_capture_requested)
            {
                return 0;
            }
            if (!wg_mouse_captured)
            {
                if (pressed && GetForegroundWindow() == window)
                {
                    WG_Win32ApplyMouseCapture(1);
                }
                return 0;
            }
            wg_queue_mouse_button(button, pressed);
            return 0;
        }

        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE)
            {
#ifdef WG_LEGACY_WIN32
                wg_legacy_mouse_position_valid = 0;
#else
                wg_raw_mouse_position_valid = 0;
#endif
                WG_Win32ApplyMouseCapture(0);
            }
            return DefWindowProc(window, message, wparam, lparam);

        case WM_MOVE:
        case WM_SIZE:
            WG_Win32UpdateMouseClip();
            return DefWindowProc(window, message, wparam, lparam);

        case WM_CLOSE:
            wg_quit_pending = 1;
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            wg_window = NULL;
            wg_quit_pending = 1;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(window, message, wparam, lparam);
    }
}

static int WG_Win32Init(void)
{
#ifndef WG_LEGACY_WIN32
    RAWINPUTDEVICE mouse;
#endif
    WNDCLASS window_class;
    RECT rectangle;
    RECT work_area;
    HINSTANCE instance;
    int window_width;
    int window_height;
    int window_x = CW_USEDEFAULT;
    int window_y = CW_USEDEFAULT;
    BYTE cursor_and_mask[32U * 32U / 8U];
    BYTE cursor_xor_mask[32U * 32U / 8U];

    instance = GetModuleHandle(NULL);
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = wg_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = wg_window_class;

    if (!RegisterClass(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return 0;
    }

    rectangle.left = 0;
    rectangle.top = 0;
    rectangle.right = WOLF3D_SCREEN_WIDTH * WG_INITIAL_SCALE;
    rectangle.bottom = (WOLF3D_SCREEN_WIDTH * WG_DISPLAY_ASPECT_HEIGHT
                        / WG_DISPLAY_ASPECT_WIDTH) * WG_INITIAL_SCALE;
    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
    window_width = rectangle.right - rectangle.left;
    window_height = rectangle.bottom - rectangle.top;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0))
    {
        window_x = work_area.left
                 + (work_area.right - work_area.left - window_width) / 2;
        window_y = work_area.top
                 + (work_area.bottom - work_area.top - window_height) / 2;
    }

    wg_window = CreateWindowEx(0, wg_window_class, TEXT("wolf3d"),
                                WS_OVERLAPPEDWINDOW, window_x, window_y,
                                window_width, window_height,
                                NULL, NULL, instance, NULL);
    if (wg_window == NULL)
    {
        return 0;
    }

    memset(cursor_and_mask, 0xff, sizeof(cursor_and_mask));
    memset(cursor_xor_mask, 0, sizeof(cursor_xor_mask));
    wg_blank_cursor = CreateCursor(instance, 0, 0, 32, 32,
                                   cursor_and_mask, cursor_xor_mask);
    if (wg_blank_cursor == NULL)
    {
        DestroyWindow(wg_window);
        wg_window = NULL;
        return 0;
    }

    wg_mouse_enabled = wg_mouse_mode > 0
        || (wg_mouse_mode < 0 && GetSystemMetrics(SM_MOUSEPRESENT) != 0);

#ifndef WG_LEGACY_WIN32
    wg_raw_mouse_position_valid = 0;
    wg_raw_mouse_wobble = 0;
    mouse.usUsagePage = 0x01U;
    mouse.usUsage = 0x02U;
    mouse.dwFlags = 0U;
    mouse.hwndTarget = wg_window;
    if (wg_mouse_enabled
        && !RegisterRawInputDevices(&mouse, 1U, sizeof(mouse)))
    {
        DestroyCursor(wg_blank_cursor);
        wg_blank_cursor = NULL;
        DestroyWindow(wg_window);
        wg_window = NULL;
        return 0;
    }
#else
    wg_legacy_mouse_position_valid = 0;
#endif

    QueryPerformanceFrequency(&wg_counter_frequency);
    QueryPerformanceCounter(&wg_counter_start);
    wg_event_read = 0;
    wg_event_write = 0;
    wg_quit_pending = 0;
    wg_fullscreen = 0;
    wg_fullscreen_enter_down = 0;
    wg_mouse_captured = 0;
    wg_mouse_capture_requested = 0;
    ZeroMemory(wg_joystick_x, sizeof(wg_joystick_x));
    ZeroMemory(wg_joystick_y, sizeof(wg_joystick_y));
    ZeroMemory(wg_joystick_buttons, sizeof(wg_joystick_buttons));
    ZeroMemory(wg_joystick_connected, sizeof(wg_joystick_connected));
    wg_load_xinput();
    ShowWindow(wg_window, SW_SHOW);
    if (wg_start_fullscreen && !WG_Win32SetFullscreen(1))
    {
        DestroyCursor(wg_blank_cursor);
        wg_blank_cursor = NULL;
        DestroyWindow(wg_window);
        wg_window = NULL;
        return 0;
    }
    return 1;
}

static void WG_Win32Shutdown(void)
{
    WG_Win32PCMShutdown();
    WG_Win32RequestMouseCapture(0);
    wg_xinput_get_state = NULL;
    if (wg_xinput_module != NULL)
    {
        FreeLibrary(wg_xinput_module);
        wg_xinput_module = NULL;
    }
    if (wg_window != NULL)
    {
        DestroyWindow(wg_window);
        wg_window = NULL;
    }
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    if (wg_blank_cursor != NULL)
    {
        DestroyCursor(wg_blank_cursor);
        wg_blank_cursor = NULL;
    }
    UnregisterClass(wg_window_class, GetModuleHandle(NULL));
    wg_write_text_screen();
}

static void WG_Win32Present(const uint8_t *pixels, const uint8_t *palette)
{
    BITMAPINFO bitmap_info;
    RECT client;
    RECT presentation;
    HDC device_context;
    size_t index;

    if (wg_window == NULL)
    {
        return;
    }

    for (index = 0; index < WOLF3D_SCREEN_WIDTH * WOLF3D_SCREEN_HEIGHT; ++index)
    {
        size_t color = (size_t)pixels[index] * 3U;
        wg_pixels[index] = ((uint32_t)palette[color] << 16)
                         | ((uint32_t)palette[color + 1U] << 8)
                         | (uint32_t)palette[color + 2U];
    }

    ZeroMemory(&bitmap_info, sizeof(bitmap_info));
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = WOLF3D_SCREEN_WIDTH;
    bitmap_info.bmiHeader.biHeight = -WOLF3D_SCREEN_HEIGHT;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    GetClientRect(wg_window, &client);
    presentation = wg_presentation_rectangle(&client);
    device_context = GetDC(wg_window);
    PatBlt(device_context, client.left, client.top,
           client.right - client.left, presentation.top - client.top,
           BLACKNESS);
    PatBlt(device_context, client.left, presentation.bottom,
           client.right - client.left, client.bottom - presentation.bottom,
           BLACKNESS);
    PatBlt(device_context, client.left, presentation.top,
           presentation.left - client.left,
           presentation.bottom - presentation.top, BLACKNESS);
    PatBlt(device_context, presentation.right, presentation.top,
           client.right - presentation.right,
           presentation.bottom - presentation.top, BLACKNESS);
    SetStretchBltMode(device_context, COLORONCOLOR);
    StretchDIBits(device_context, presentation.left, presentation.top,
                  presentation.right - presentation.left,
                  presentation.bottom - presentation.top,
                  0, 0, WOLF3D_SCREEN_WIDTH, WOLF3D_SCREEN_HEIGHT, wg_pixels,
                  &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(wg_window, device_context);
}

static uint32_t WG_Win32GetTicksMs(void)
{
    LARGE_INTEGER now;
    uint64_t ticks;

    QueryPerformanceCounter(&now);
    ticks = (uint64_t)(now.QuadPart - wg_counter_start.QuadPart);
    return (uint32_t)((ticks * 1000U) / (uint64_t)wg_counter_frequency.QuadPart);
}

static void WG_Win32SleepMs(uint32_t milliseconds)
{
    Sleep(milliseconds);
}

static int WG_Win32PollEvent(wolf3d_event_t *event)
{
    MSG message;

    if (event == NULL)
    {
        return 0;
    }

    if (wg_quit_pending)
    {
        wg_quit_pending = 0;
        event->type = WOLF3D_EVENT_QUIT;
        return 1;
    }

    /* Drain translated input before dispatching more Windows messages.  A
       single message can enqueue only a small group of raw-mouse events, so
       returning as soon as that group exists keeps the fixed queue from ever
       being crowded by a host-message backlog.  In particular, key/button
       releases cannot be discarded behind accumulated mouse motion. */
    if (wg_event_read != wg_event_write)
    {
        *event = wg_event_queue[wg_event_read];
        wg_event_read = (wg_event_read + 1U) % WOLF3D_EVENT_QUEUE_CAPACITY;
        return 1;
    }

    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            event->type = WOLF3D_EVENT_QUIT;
            return 1;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
        if (wg_event_read != wg_event_write)
        {
            *event = wg_event_queue[wg_event_read];
            wg_event_read = (wg_event_read + 1U) % WOLF3D_EVENT_QUEUE_CAPACITY;
            return 1;
        }
    }

    wg_poll_joysticks();

    if (wg_event_read != wg_event_write)
    {
        *event = wg_event_queue[wg_event_read];
        wg_event_read = (wg_event_read + 1U) % WOLF3D_EVENT_QUEUE_CAPACITY;
        return 1;
    }

    event->type = WOLF3D_EVENT_NONE;
    return 0;
}

static int WG_Win32IsInteractive(void)
{
    return 1;
}

static uint32_t WG_Win32InputDevices(void)
{
    uint32_t devices = wg_mouse_enabled
        ? WOLF3D_INPUT_DEVICE_MOUSE : 0U;
    DWORD joystick;

    if (wg_joystick_mode > 0)
    {
        devices |= WOLF3D_INPUT_DEVICE_JOYSTICK;
    }
    else if (wg_joystick_mode < 0 && wg_xinput_get_state != NULL)
    {
        for (joystick = 0U; joystick < WOLF3D_MAX_JOYSTICKS; ++joystick)
        {
            wg_xinput_state_t state;

            ZeroMemory(&state, sizeof(state));
            if (wg_xinput_get_state(joystick, &state) == ERROR_SUCCESS)
            {
                devices |= WOLF3D_INPUT_DEVICE_JOYSTICK;
                break;
            }
        }
    }
    return devices;
}

static void WG_Win32SetWindowTitle(const char *title)
{
#ifdef WG_WIN9X
    if (wg_window != NULL)
    {
        SetWindowTextA(wg_window, title);
    }
#else
    wchar_t wide_title[256];

    if (wg_window == NULL)
    {
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title,
                        (int)(sizeof(wide_title) / sizeof(wide_title[0])));
    SetWindowTextW(wg_window, wide_title);
#endif
}

static void WG_Win32ReportError(const char *message)
{
    HANDLE output = wg_console_output_handle();

    if (output != NULL)
    {
        DWORD written;
        static const char prefix[] = "wolf3d: ";
        static const char newline[] = "\r\n";

        (void)WriteFile(output, prefix, (DWORD)(sizeof(prefix) - 1U),
                        &written, NULL);
        (void)WriteFile(output, message, (DWORD)strlen(message),
                        &written, NULL);
        (void)WriteFile(output, newline, (DWORD)(sizeof(newline) - 1U),
                        &written, NULL);
    }
    MessageBoxA(wg_window, message, "wolf3d", MB_OK | MB_ICONERROR);
}

static void WG_Win32PrintMessage(const char *message)
{
    HANDLE output = wg_console_output_handle();

    if (output != NULL)
    {
        DWORD written;
        static const char newline[] = "\r\n";

        (void)WriteFile(output, message, (DWORD)strlen(message),
                        &written, NULL);
        (void)WriteFile(output, newline, (DWORD)(sizeof(newline) - 1U),
                        &written, NULL);
    }
    else
    {
        fprintf(stdout, "%s\n", message);
        fflush(stdout);
    }
}

static void WG_Win32PresentText(const uint8_t *cells, uint16_t columns,
                                uint16_t rows)
{
    size_t size = (size_t)columns * rows * WOLF3D_TEXT_CELL_BYTES;

    if (cells == NULL || columns > WOLF3D_TEXT_COLUMNS || rows > WOLF3D_TEXT_ROWS
        || size > sizeof(wg_text_screen))
    {
        return;
    }
    memcpy(wg_text_screen, cells, size);
    wg_text_columns = columns;
    wg_text_rows = rows;
}

static int WG_Win32PCMInit(uint32_t sample_rate, uint16_t channels)
{
    WAVEFORMATEX format;
    size_t index;

    if (sample_rate == 0U || channels != 2U)
    {
        return 0;
    }
    WG_Win32PCMShutdown();
    ZeroMemory(&format, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = sample_rate;
    format.wBitsPerSample = 16U;
    format.nBlockAlign = (WORD)(channels * sizeof(int16_t));
    format.nAvgBytesPerSec = sample_rate * format.nBlockAlign;
    if (waveOutOpen(&wg_wave_out, WAVE_MAPPER, &format, 0, 0,
                    CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        wg_wave_out = NULL;
        return 0;
    }
    ZeroMemory(wg_wave_headers, sizeof(wg_wave_headers));
    ZeroMemory(wg_wave_used, sizeof(wg_wave_used));
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        wg_wave_headers[index].lpData = (LPSTR)wg_wave_samples[index];
        wg_wave_headers[index].dwBufferLength =
            (DWORD)sizeof(wg_wave_samples[index]);
        if (waveOutPrepareHeader(wg_wave_out, &wg_wave_headers[index],
                                 sizeof(wg_wave_headers[index]))
            != MMSYSERR_NOERROR)
        {
            WG_Win32PCMShutdown();
            return 0;
        }
    }
    return 1;
}

static int WG_Win32PCMInitEx(const wolf3d_pcm_format_t *requested,
                             wolf3d_pcm_format_t *obtained)
{
    if (requested == NULL || obtained == NULL
        || requested->bits_per_sample != 16U
        || !WG_Win32PCMInit(requested->sample_rate, requested->channels))
    {
        return 0;
    }
    *obtained = *requested;
    return 1;
}

static void WG_Win32PCMShutdown(void)
{
    size_t index;

    if (wg_wave_out == NULL)
    {
        return;
    }
    waveOutReset(wg_wave_out);
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        if ((wg_wave_headers[index].dwFlags & WHDR_PREPARED) != 0U)
        {
            waveOutUnprepareHeader(wg_wave_out, &wg_wave_headers[index],
                                   sizeof(wg_wave_headers[index]));
        }
    }
    waveOutClose(wg_wave_out);
    wg_wave_out = NULL;
    ZeroMemory(wg_wave_headers, sizeof(wg_wave_headers));
    ZeroMemory(wg_wave_used, sizeof(wg_wave_used));
}

static size_t WG_Win32PCMWritableFrames(void)
{
    size_t index;

    if (wg_wave_out == NULL)
    {
        return 0U;
    }
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        if (wg_wave_used[index]
            && (wg_wave_headers[index].dwFlags & WHDR_DONE) != 0U)
        {
            wg_wave_used[index] = 0U;
        }
        if (!wg_wave_used[index])
        {
            return WG_PCM_BUFFER_FRAMES;
        }
    }
    return 0U;
}

static int WG_Win32PCMSubmit(const int16_t *samples, size_t frame_count)
{
    size_t index;

    if (wg_wave_out == NULL || samples == NULL || frame_count == 0U
        || frame_count > WG_PCM_BUFFER_FRAMES)
    {
        return 0;
    }
    for (index = 0U; index < WG_PCM_BUFFER_COUNT; ++index)
    {
        if (!wg_wave_used[index])
        {
            size_t byte_count = frame_count * 2U * sizeof(*samples);

            memcpy(wg_wave_samples[index], samples, byte_count);
            wg_wave_headers[index].dwBufferLength = (DWORD)byte_count;
            wg_wave_headers[index].dwFlags &= ~WHDR_DONE;
            if (waveOutWrite(wg_wave_out, &wg_wave_headers[index],
                             sizeof(wg_wave_headers[index]))
                != MMSYSERR_NOERROR)
            {
                return 0;
            }
            wg_wave_used[index] = 1U;
            return 1;
        }
    }
    return 0;
}

int WG_InstallPlatform(void)
{
    static const wolf3d_platform_api_t platform =
    {
        WOLF3D_PLATFORM_API_VERSION,
        sizeof(wolf3d_platform_api_t),
        WG_Win32Init,
        WG_Win32Shutdown,
        WG_Win32Present,
        WG_Win32GetTicksMs,
        WG_Win32SleepMs,
        WG_Win32PollEvent,
        WG_Win32IsInteractive,
        WG_Win32SetWindowTitle,
        WG_Win32PrintMessage,
        WG_Win32ReportError,
        WG_Win32PresentText,
        WG_Win32PCMInit,
        WG_Win32PCMShutdown,
        WG_Win32PCMWritableFrames,
        WG_Win32PCMSubmit,
        WG_Win32PCMInitEx,
#ifdef WG_WIN9X
        WG_DOSAdLibInit,
        WG_DOSAdLibShutdown,
        WG_DOSAdLibWrite,
#else
        NULL,
        NULL,
        NULL,
#endif
        WG_Win32InputDevices,
        WG_Win32RequestMouseCapture
    };

    return wolf3d_SetPlatform(&platform) == WOLF3D_RESULT_OK;
}

static void WG_Win32DiagnosticReport(char *report, size_t report_size)
{
    unsigned joystick_count = 0U;
    unsigned xinput_count = 0U;
#ifdef WG_WIN9X
    int monitor_count = 1;
#else
    int monitor_count = GetSystemMetrics(SM_CMONITORS);
#endif
    UINT joystick;
    JOYINFO info;

    for (joystick = 0U; joystick < joyGetNumDevs(); ++joystick)
    {
        if (joyGetPos(joystick, &info) == JOYERR_NOERROR)
        {
            ++joystick_count;
        }
    }
    wg_load_xinput();
    if (wg_xinput_get_state != NULL)
    {
        DWORD slot;

        for (slot = 0U; slot < WOLF3D_MAX_JOYSTICKS; ++slot)
        {
            wg_xinput_state_t state;

            memset(&state, 0, sizeof(state));
            if (wg_xinput_get_state(slot, &state) == ERROR_SUCCESS)
            {
                ++xinput_count;
            }
        }
    }
    if (wg_xinput_module != NULL)
    {
        FreeLibrary(wg_xinput_module);
        wg_xinput_module = NULL;
        wg_xinput_get_state = NULL;
    }
    (void)report_size;
    (void)sprintf(report,
        "  Video: %d x %d desktop, %d monitor(s)\n"
        "  Audio: %u waveOut device(s)\n"
        "  Mouse: %s\n"
        "  Joystick: %u legacy device(s), %u XInput controller(s) connected\n",
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        monitor_count, (unsigned)waveOutGetNumDevs(),
        GetSystemMetrics(SM_MOUSEPRESENT) ? "present" : "not detected",
        joystick_count, xinput_count);
}

#ifdef WG_WIN9X
int main(int argc, char **argv)
#else
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    LPWSTR command_line, int show_command)
#endif
{
#ifdef WG_WIN9X
    char module_path[MAX_PATH];
    DWORD module_path_length;
#else
    int argc;
    char **argv;
    wchar_t **wide_argv;
    wchar_t module_path[32768];
    const wchar_t *wide_argument;
    DWORD module_path_length;
#endif
    int launcher_argc;
    char **launcher_argv;
    int index;
    int exit_code = 1;
    char launcher_error[2048];
    wg_launcher_arguments_t launcher_arguments;
    wg_game_arguments_t game_arguments;
    wolf3d_result_t result;

#ifndef WG_WIN9X
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;
#endif

    if (!WG_InstallPlatform())
    {
        return 1;
    }

#ifdef WG_WIN9X
    module_path_length = GetModuleFileNameA(NULL, module_path, sizeof(module_path));
    if (argc > 0 && module_path_length > 0U
        && module_path_length < sizeof(module_path))
    {
        argv[0] = module_path;
    }
#else
    wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (wide_argv == NULL)
    {
        return 1;
    }

    module_path_length = GetModuleFileNameW(NULL, module_path,
                                           (DWORD)(sizeof(module_path)
                                                   / sizeof(module_path[0])));

    argv = (char **)LocalAlloc(LMEM_FIXED, (size_t)argc * sizeof(*argv));
    if (argv == NULL)
    {
        LocalFree(wide_argv);
        return 1;
    }

    for (index = 0; index < argc; ++index)
    {
        int bytes;

        wide_argument = index == 0 && module_path_length > 0U
                                && module_path_length
                                       < (DWORD)(sizeof(module_path)
                                                 / sizeof(module_path[0]))
                            ? module_path : wide_argv[index];
        bytes = WideCharToMultiByte(CP_UTF8, 0, wide_argument, -1,
                                    NULL, 0, NULL, NULL);
        argv[index] = (char *)LocalAlloc(LMEM_FIXED, (size_t)bytes);
        if (argv[index] == NULL)
        {
            while (index-- > 0)
            {
                LocalFree(argv[index]);
            }
            LocalFree(argv);
            LocalFree(wide_argv);
            return 1;
        }
        WideCharToMultiByte(CP_UTF8, 0, wide_argument, -1,
                            argv[index], bytes, NULL, NULL);
    }
#endif

    if (!WG_LoadLauncherArguments(argc, argv, &launcher_arguments,
                                  launcher_error, sizeof(launcher_error)))
    {
        WG_PrintLauncherError(launcher_error);
        result = WOLF3D_RESULT_PLATFORM_ERROR;
        goto cleanup;
    }
    launcher_argc = launcher_arguments.argc;
    launcher_argv = launcher_arguments.argv;
    for (index = 1; index < launcher_argc; ++index)
    {
        if (strcmp(launcher_argv[index], "--fullscreen") == 0)
        {
            wg_start_fullscreen = 1;
        }
        else if (strcmp(launcher_argv[index], "--windowed") == 0)
        {
            wg_start_fullscreen = 0;
        }
        else if (strcmp(launcher_argv[index], "--mouse") == 0)
        {
            wg_mouse_mode = 1;
        }
        else if (strcmp(launcher_argv[index], "--nomouse") == 0)
        {
            wg_mouse_mode = 0;
        }
        else if (strcmp(launcher_argv[index], "--joy") == 0)
        {
            wg_joystick_mode = 1;
        }
        else if (strcmp(launcher_argv[index], "--nojoy") == 0)
        {
            wg_joystick_mode = 0;
        }
    }

    if (WG_CommandLineHelpRequested(launcher_argc, launcher_argv))
    {
        WG_PrintCommandLineHelp(
            launcher_argc > 0 ? launcher_argv[0] : "wolf3d.exe",
            "Win32 host options:\n"
            "  --fullscreen         Start in borderless fullscreen mode\n"
            "  --windowed           Start in windowed mode\n",
            "Press F11 or Alt+Enter to toggle windowed/fullscreen mode.\n");
        result = WOLF3D_RESULT_QUIT;
    }
    else if (WG_CommandLineDiagnosticsRequested(launcher_argc,
                                                launcher_argv))
    {
        char report[1024];

        WG_Win32DiagnosticReport(report, sizeof(report));
        WG_PrintDiagnostics(launcher_argc, launcher_argv,
                            launcher_arguments.config_path, report);
        result = WOLF3D_RESULT_QUIT;
    }
    else
    {
        if (!WG_PrepareGameArguments(launcher_argc, launcher_argv,
                                     &game_arguments,
                                     launcher_error,
                                     sizeof(launcher_error)))
        {
            WG_PrintLauncherError(launcher_error);
            result = WOLF3D_RESULT_PLATFORM_ERROR;
        }
        else
        {
            result = wolf3d_Create(game_arguments.argc,
                                   game_arguments.argv);
            WG_FreeGameArguments(&game_arguments);
        }
    }
    if (result == WOLF3D_RESULT_OK)
    {
        result = wolf3d_Run();
        wolf3d_Shutdown();
    }
    exit_code = result == WOLF3D_RESULT_NOT_IMPLEMENTED || result == WOLF3D_RESULT_QUIT
                    ? 0 : 1;

    WG_FreeLauncherArguments(&launcher_arguments);

cleanup:
#ifndef WG_WIN9X
    for (index = 0; index < argc; ++index)
    {
        LocalFree(argv[index]);
    }
    LocalFree(argv);
    LocalFree(wide_argv);
#endif
    return result == WOLF3D_RESULT_PLATFORM_ERROR ? 1 : exit_code;
}
