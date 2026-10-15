#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "../WG_ENGINE_COMPAT.h"
#include "../WG_HELP.h"
#include "../WG_FULLSCREEN.h"
static wg_fullscreen_options_t fullscreen_options={-1,0,0};
#include "../WG_TEXT_OUTPUT.h"
#include "WG_SDL1_HEADERS.h"
#ifdef WG_SDL1_DYNAMIC
#include "WG_SDL1_DYNAMIC.h"
#define WG_InstallPlatform WG_SDL1InstallPlatform
#endif
#ifdef _WIN32
#undef main
#include <windows.h>
#include "../win32/WG_WAVEOUT.h"
static wg_waveout_t pcm_wave;
static int pcm_output_wave;
static void ui_shutdown(void);
#define HOST_SHUTDOWN ui_shutdown
#else
#define HOST_SHUTDOWN shutdown_host
#endif
#ifdef WG_INPOUT32
#include "../win32/WG_INPOUT32.h"
#endif
#if defined(WG_INPOUT32) || defined(WG_WIN9X)
#define WG_SDL1_OPL_WORKER
#include "../win32/WG_OPL_PLAYBACK.h"
#include "../dos/WG_DOS_ADLIB.h"
static unsigned pcm_rate;
static int pcm_native;
static void opl_shutdown(void) {WG_OPLPlaybackStop();pcm_native=0;WG_DOSAdLibShutdown();}
static int opl_init(void) {
    if(!WG_DOSAdLibInit())return 0;
    if(WG_OPLPlaybackStart(pcm_output_wave?pcm_wave.handle:NULL,pcm_rate)) {pcm_native=1;return 1;}
    WG_DOSAdLibShutdown();return 0;
}
#endif
#ifdef WG_LINUX_OPL
#include "../linux/WG_LINUX_OPL.h"
#define WG_SDL1_OPL_NOTE "Native OPL: direct ports 0x388-0x38b; RH9 elapsed-time waits, hardware confirmation pending"
#elif defined(WG_INPOUT32)
#define WG_SDL1_OPL_NOTE "Native OPL: InpOut32, timestamped playback"
#elif defined(WG_WIN9X)
#define WG_SDL1_OPL_NOTE "Native OPL: direct ISA ports on Windows 95/98/Me, timestamped playback"
#else
#define WG_SDL1_OPL_NOTE "Native OPL: not compiled"
#endif
#ifdef _WIN32
#define WG_SDL1_VIDEO_HELP "Windows options:\n  --sdl-video directx|windib  Default directx; falls back to windib\n  --sdl-scale hardware|software  Default hardware; falls back to software\n"
#else
#define WG_SDL1_VIDEO_HELP "Linux: X11 video and OSS PCM.\n"
#endif
#include <stdio.h>
#include "../WG_PRINTF_COMPAT.h"
#include <stdlib.h>
#include <string.h>
#include "WG_SDL1_GL.h"

/* The engine stays on the main thread. SDL's callback only drains this queue. */
#define WG_QUEUE_FRAMES 16384U
static int16_t *pcm;
static size_t pcm_read, pcm_count, pcm_limit, pcm_capacity;
static unsigned pcm_channels;
static int pcm_open;
static int pcm_output_choice;
static unsigned pcm_callback_samples;
/* Only collected by the short silent --diag probe, under SDL's audio lock. */
static int pcm_probe_active;
static unsigned long pcm_probe_calls,pcm_probe_requested,pcm_probe_consumed;
static int sdl_diagnostics;
static char sdl_report[2048];
static SDL_Surface *screen;
static SDL_Surface *software_frame;
static int scale = 2, fullscreen, mouse = 1, joy = 1, capture, focused = 1;
static SDL_Joystick *joysticks[WOLF3D_MAX_JOYSTICKS];
static int16_t joy_x[WOLF3D_MAX_JOYSTICKS], joy_y[WOLF3D_MAX_JOYSTICKS];
static uint32_t joy_buttons[WOLF3D_MAX_JOYSTICKS];
static unsigned char joy_dirty[WOLF3D_MAX_JOYSTICKS], keys[256];

#ifdef _WIN32
#include "WG_SDL1_DDRAW.h"
#define WG_DISPLAY_STATE_ONLY
#include "../win32/WG_WIN32_DISPLAY.h"
static wg_display_mode_t display_mode;
static const char *video_driver_choice;
static RECT windowed_rect;
static int windowed_rect_valid, borderless_active;
static int set_video_driver(const char *value)
{
    if(!strcmp(value,"directx"))video_driver_choice="directx";
    else if(!strcmp(value,"windib"))video_driver_choice="windib";
    else return 0;
    return 1;
}
static int set_scaling(const char *value)
{
    if(!strcmp(value,"hardware"))dd_software_requested=0;
    else if(!strcmp(value,"software"))dd_software_requested=1;
    else return 0;
    return 1;
}
static const char *requested_video_driver(void)
{
    const char *environment=getenv("SDL_VIDEODRIVER");
    return video_driver_choice?video_driver_choice:
        (environment && *environment?environment:"directx");
}
static int start_video_with_fallback(const char *driver,int (*start)(const char *))
{
    if(start(driver))return 1;
    if(strcmp(driver,"directx"))return 0;
    return start("windib");
}
#endif

static uint16_t scan(SDLKey key)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyz";
    static const uint8_t codes[] = {0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c};
    unsigned i;
    for (i=0; i<26; ++i) if (key == (SDLKey)letters[i]) return codes[i];
    if (key >= SDLK_1 && key <= SDLK_9) return (uint16_t)(2 + key - SDLK_1);
    if (key >= SDLK_F1 && key <= SDLK_F10) return (uint16_t)(0x3b + key - SDLK_F1);
    switch(key) {
    case SDLK_ESCAPE:return 1; case SDLK_0:return 0x0b;
    case SDLK_MINUS:return 0x0c; case SDLK_EQUALS:return 0x0d;
    case SDLK_BACKSPACE:return 0x0e; case SDLK_TAB:return 0x0f;
    case SDLK_LEFTBRACKET:return 0x1a; case SDLK_RIGHTBRACKET:return 0x1b;
    case SDLK_RETURN:case SDLK_KP_ENTER:return 0x1c;
    case SDLK_LCTRL:case SDLK_RCTRL:return 0x1d;
    case SDLK_SEMICOLON:return 0x27; case SDLK_QUOTE:return 0x28;
    case SDLK_BACKQUOTE:return 0x29; case SDLK_LSHIFT:return 0x2a;
    case SDLK_BACKSLASH:return 0x2b; case SDLK_COMMA:return 0x33;
    case SDLK_PERIOD:return 0x34; case SDLK_SLASH:case SDLK_KP_DIVIDE:return 0x35;
    case SDLK_RSHIFT:return 0x36; case SDLK_KP_MULTIPLY:return 0x37;
    case SDLK_LALT:case SDLK_RALT:return 0x38; case SDLK_SPACE:return 0x39;
    case SDLK_CAPSLOCK:return 0x3a; case SDLK_NUMLOCK:return 0x45;
    case SDLK_SCROLLOCK:return 0x46; case SDLK_HOME:case SDLK_KP7:return 0x47;
    case SDLK_UP:case SDLK_KP8:return 0x48; case SDLK_PAGEUP:case SDLK_KP9:return 0x49;
    case SDLK_KP_MINUS:return 0x4a; case SDLK_LEFT:case SDLK_KP4:return 0x4b;
    case SDLK_KP5:return 0x4c; case SDLK_RIGHT:case SDLK_KP6:return 0x4d;
    case SDLK_KP_PLUS:return 0x4e; case SDLK_END:case SDLK_KP1:return 0x4f;
    case SDLK_DOWN:case SDLK_KP2:return 0x50; case SDLK_PAGEDOWN:case SDLK_KP3:return 0x51;
    case SDLK_INSERT:case SDLK_KP0:return 0x52; case SDLK_DELETE:case SDLK_KP_PERIOD:return 0x53;
    case SDLK_PAUSE:return WOLF3D_KEY_PAUSE; default:return 0;
    }
}
static void mouse_capture(int value)
{
    capture = value;
    SDL_WM_GrabInput(value && mouse && focused ? SDL_GRAB_ON : SDL_GRAB_OFF);
    SDL_ShowCursor(focused && (fullscreen || (value && mouse)) ? SDL_DISABLE : SDL_ENABLE);
#ifdef _WIN32
    if(!mouse || !focused)ClipCursor(NULL);
#endif
}
static int next_event(SDL_Event *event)
{
    int result=SDL_PollEvent(event);
#ifdef _WIN32
    /* SDL1 DirectInput can clip to a single pixel while pumping mouse events,
       even with SDL_GRAB_OFF. --nomouse must leave the desktop cursor free. */
    if(!mouse || !focused)ClipCursor(NULL);
#endif
    return result;
}
static int video(int width, int height)
{
    SDL_Surface *next;
    Uint32 render_flags=SDL_SWSURFACE;
    int want_gl=render_choice==1;
#ifdef _WIN32
    SDL_SysWMinfo wm;
#endif
#ifndef _WIN32
    want_gl=render_choice!=2;
#endif
    gl_close();
    if(want_gl && gl_library_path && !gl_library_loaded) {
      if(SDL_GL_LoadLibrary(gl_library_path)<0) {
        fprintf(stderr,"SDL1 OpenGL library %s: %s\n",gl_library_path,SDL_GetError());return 0;
      }
      gl_library_loaded=1;
    }
    if(want_gl) {
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL,gl_acceleration);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
        render_flags=SDL_OPENGL;
    }
#ifdef _WIN32
    SDL_VERSION(&wm.version);
    if(fullscreen && !borderless_active && screen && SDL_GetWMInfo(&wm))
        windowed_rect_valid=GetWindowRect(wm.window,&windowed_rect)!=0;
    dd_close();dd_disabled=0;
    if(fullscreen && WG_FullscreenDisplay(&fullscreen_options)) {
        display_mode.width=fullscreen_options.width?fullscreen_options.width:(unsigned)GetSystemMetrics(SM_CXSCREEN);
        display_mode.height=fullscreen_options.height?fullscreen_options.height:(unsigned)GetSystemMetrics(SM_CYSCREEN);
        if(focused && !WG_DisplayEnable(&display_mode)) {
            (void)WG_DisplayRestore(&display_mode,1);
            fprintf(stderr,"SDL1: requested display mode unavailable\n");return 0;
        }
    } else if(!WG_DisplayRestore(&display_mode,1))return 0;
    if(fullscreen) {width=GetSystemMetrics(SM_CXSCREEN);height=GetSystemMetrics(SM_CYSCREEN);}
    /* Stay in normal DirectDraw cooperative mode. SDL_FULLSCREEN takes exclusive
       ownership and prevents our separate hardware-stretch device from working. */
    next = SDL_SetVideoMode(width,height,32,
        render_flags | (fullscreen ? SDL_NOFRAME : SDL_RESIZABLE));
#else
    if(fullscreen) {
        const SDL_VideoInfo *info=SDL_GetVideoInfo();
        width=WG_FullscreenDisplay(&fullscreen_options) && fullscreen_options.width?(int)fullscreen_options.width:info->current_w;
        height=WG_FullscreenDisplay(&fullscreen_options) && fullscreen_options.height?(int)fullscreen_options.height:info->current_h;
    }
    next = SDL_SetVideoMode(width, height, 32,
        render_flags | (fullscreen ? (WG_FullscreenDisplay(&fullscreen_options)?SDL_FULLSCREEN:SDL_NOFRAME) : SDL_RESIZABLE));
#endif
    if(want_gl && (!next || !gl_open())) {
        gl_close();
        if(render_choice==0) {
            int saved=render_choice,ok;
            render_choice=2;ok=video(width,height);render_choice=saved;return ok;
        }
        fprintf(stderr,"SDL1 OpenGL initialization failed: %s\n",SDL_GetError());
        return 0;
    }
    if (!next) {
#ifdef _WIN32
        (void)WG_DisplayRestore(&display_mode,1);
#endif
        fprintf(stderr, "SDL 1.2 video: %s\n", SDL_GetError());return 0;
    }
    if(!gl_active && next->format->BytesPerPixel != 4) {
        fprintf(stderr, "SDL 1.2 video requires a 32-bit software surface\n");
        return 0;
    }
    screen = next;
#ifdef _WIN32
    SDL_VERSION(&wm.version);
    if(SDL_GetWMInfo(&wm)) {
        if(fullscreen)SetWindowPos(wm.window,focused?HWND_TOPMOST:HWND_NOTOPMOST,0,0,width,height,SWP_FRAMECHANGED);
        else if(borderless_active && !windowed_rect_valid)
            SetWindowPos(wm.window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
        else if(borderless_active && windowed_rect_valid)
            SetWindowPos(wm.window,HWND_NOTOPMOST,windowed_rect.left,windowed_rect.top,0,0,SWP_NOSIZE);
    }
    borderless_active=fullscreen;
#endif
    if(software_frame)SDL_FreeSurface(software_frame);
    software_frame=NULL;
    /* Software surfaces retain their borders between flips. */
    if(!gl_active)SDL_FillRect(screen,NULL,0);
    mouse_capture(capture);
    return 1;
}
static void pcm_shutdown(void)
{
#ifdef WG_SDL1_OPL_WORKER
    opl_shutdown();pcm_rate=0;
#endif
#ifdef WG_LINUX_OPL
    WG_LinuxOPLShutdown();
    WG_LinuxOPLSetRate(0);
#endif
#ifdef _WIN32
    WG_WaveOutClose(&pcm_wave);pcm_output_wave=0;
#endif
    if (pcm_open) SDL_CloseAudio();
    free(pcm);pcm=NULL;pcm_capacity=pcm_limit=0;
    pcm_open = 0; pcm_count = pcm_read = 0;
}
static void shutdown_host(void)
{
    unsigned i;
    pcm_shutdown();
    if(SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_WM_GrabInput(SDL_GRAB_OFF);
        SDL_ShowCursor(SDL_ENABLE);
    }
    capture=0;
    for(i=0;i<WOLF3D_MAX_JOYSTICKS;++i) {
        if(joysticks[i]) SDL_JoystickClose(joysticks[i]);
        joysticks[i]=NULL;
    }
#ifdef _WIN32
    dd_close();
    if(!WG_DisplayRestore(&display_mode,1))fprintf(stderr,"SDL1: could not restore desktop display mode\n");
#endif
    gl_close();SDL_Quit(); screen=NULL;gl_library_loaded=0;
#ifdef _WIN32
    /* Release after SDL teardown, which can pump additional DirectInput events. */
    ClipCursor(NULL);
    borderless_active=windowed_rect_valid=0;
#endif
    if(software_frame)SDL_FreeSurface(software_frame);
    software_frame=NULL;
}
#ifdef _WIN32
static int try_video_driver(const char *driver)
{
    static char environment[96];
    sprintf(environment,"SDL_VIDEODRIVER=%s",driver);SDL_putenv(environment);
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)<0) {SDL_Quit();return 0;}
    if(!video(320*scale,240*scale)) {shutdown_host();return 0;}
    return 1;
}
#endif
static int init(void)
{
    unsigned i;
#ifndef _WIN32
    static char software_env[]="LIBGL_ALWAYS_SOFTWARE=1";
    static char hardware_env[]="LIBGL_ALWAYS_SOFTWARE=0";
    if(gl_acceleration>=0)SDL_putenv(gl_acceleration?hardware_env:software_env);
#endif
#ifdef _WIN32
    /* Copy before SDL_putenv replaces the environment backing store. */
    char driver[64];const char *requested=requested_video_driver();
    /* SDL 1.2 provides Windows GL contexts through windib. */
    if(render_choice==1)requested="windib";
    if(strlen(requested)>=sizeof(driver))return 0;
    strcpy(driver,requested);
    if(!start_video_with_fallback(driver,try_video_driver)) {
        fprintf(stderr,"SDL 1.2 video initialization failed: %s\n",SDL_GetError());return 0;
    }
#else
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {fprintf(stderr,"SDL 1.2 initialization: %s\n",SDL_GetError());return 0;}
    if(!video(320*scale,240*scale)) { shutdown_host(); return 0; }
#endif
    if(sdl_diagnostics) {
        char reported_driver[64];const char *presentation="SDL software surfaces";
        SDL_VideoDriverName(reported_driver,sizeof(reported_driver));
#ifdef _WIN32
        if(!gl_active && dd_open())presentation="DirectDraw hardware stretch";
#endif
        snprintf(sdl_report,sizeof(sdl_report),"  SDL video driver: %s\n  Presentation: %s\n%s",
            reported_driver,gl_active?"OpenGL":presentation,gl_active?gl_report:"");
#ifdef _WIN32
        if(!gl_active && !dd_device) {
            size_t used=strlen(sdl_report);
            snprintf(sdl_report+used,sizeof(sdl_report)-used,"  Surface fallback: %s\n",dd_reason);
        }
#endif
    }
    SDL_WM_SetCaption("wolf3d", "wolf3d");
    SDL_EnableKeyRepeat(0,0);
    memset(keys,0,sizeof(keys));
    if(joy && SDL_InitSubSystem(SDL_INIT_JOYSTICK)==0) {
        for(i=0;i<WOLF3D_MAX_JOYSTICKS && (int)i<SDL_NumJoysticks();++i) {
            joysticks[i]=SDL_JoystickOpen((int)i); joy_dirty[i]=1;
        }
    }
    return 1;
}
static void present(const uint8_t *pixels, const uint8_t *palette)
{
    Uint32 colors[256];int x,y,w,h,left,top;SDL_Rect target;
    if(!screen || !pixels || !palette)return;
    w=screen->w;h=w*3/4;if(h>screen->h) {h=screen->h;w=h*4/3;}
    if(w<1 || h<1)return;
    left=(screen->w-w)/2;top=(screen->h-h)/2;
    if(gl_active) {gl_present(pixels,palette,left,top,w,h,screen->h);return;}
#ifdef _WIN32
    if(dd_present(pixels,palette,left,top,w,h))return;
#endif
    if(!software_frame)software_frame=SDL_CreateRGBSurface(SDL_SWSURFACE,320,200,32,
        screen->format->Rmask,screen->format->Gmask,screen->format->Bmask,0);
    if(!software_frame)return;
    for(x=0;x<256;++x)colors[x]=SDL_MapRGB(software_frame->format,palette[x*3],palette[x*3+1],palette[x*3+2]);
    for(y=0;y<200;++y) {
        Uint32 *row=(Uint32 *)((Uint8 *)software_frame->pixels+y*software_frame->pitch);
        for(x=0;x<320;++x)row[x]=colors[pixels[y*320+x]];
    }
    target.x=(Sint16)left;target.y=(Sint16)top;target.w=(Uint16)w;target.h=(Uint16)h;
    if(SDL_SoftStretch(software_frame,NULL,screen,&target)==0)SDL_Flip(screen);
}
static uint32_t ticks(void) { return SDL_GetTicks(); }
static void sleep_ms(uint32_t ms) { SDL_Delay(ms); }
static int interactive(void) { return 1; }
static void title(const char *text) { SDL_WM_SetCaption(text,text); }
static void message(const char *text) { fprintf(stdout,"%s\n",text); }
static void error(const char *text) { fprintf(stderr,"wolf3d: %s\n",text); }
static void text_screen(const uint8_t *cells,uint16_t columns,uint16_t rows)
{
    HOST_SHUTDOWN();
#ifdef _WIN32
    WG_WriteWindowsTextScreen(cells,columns,rows);
#else
    WG_WriteTextScreen(stdout,cells,columns,rows,WG_TextOutputSupportsColor(stdout));
#endif
}
static uint32_t devices(void)
{
    unsigned i; uint32_t flags=mouse ? WOLF3D_INPUT_DEVICE_MOUSE : 0;
    for(i=0;i<WOLF3D_MAX_JOYSTICKS;++i) if(joysticks[i]) flags|=WOLF3D_INPUT_DEVICE_JOYSTICK;
    return flags;
}
static int poll_event(wolf3d_event_t *out)
{
    SDL_Event event;
    unsigned i;
    memset(out,0,sizeof(*out));
    if(!focused) for(i=0;i<256;++i) if(keys[i]) {
        keys[i]=0; out->type=WOLF3D_EVENT_KEY; out->key=(uint16_t)i; return 1;
    }
    while(next_event(&event)) {
        switch(event.type) {
        case SDL_QUIT:out->type=WOLF3D_EVENT_QUIT;return 1;
        case SDL_VIDEORESIZE: if(!video(event.resize.w,event.resize.h)) {out->type=WOLF3D_EVENT_QUIT;return 1;} break;
        case SDL_ACTIVEEVENT:
            if(event.active.state & SDL_APPINPUTFOCUS) {
                focused=event.active.gain;
#ifdef _WIN32
                if(fullscreen) {
                    SDL_SysWMinfo wm;SDL_VERSION(&wm.version);
                    if(SDL_GetWMInfo(&wm))SetWindowPos(wm.window,focused?HWND_TOPMOST:HWND_NOTOPMOST,
                        0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                }
                if(fullscreen && WG_FullscreenDisplay(&fullscreen_options) && !display_mode.changing) {
                    if(!focused) {
                        SDL_SysWMinfo wm;dd_close();dd_disabled=0;
                        (void)WG_DisplayRestore(&display_mode,0);
                        SDL_VERSION(&wm.version);if(SDL_GetWMInfo(&wm))ShowWindow(wm.window,SW_MINIMIZE);
                    } else if(!display_mode.active && !video(320*scale,240*scale)) {
                        out->type=WOLF3D_EVENT_QUIT;return 1;
                    }
                }
#endif
                mouse_capture(capture);
            }
            break;
        case SDL_KEYDOWN:case SDL_KEYUP:
            if(event.key.keysym.sym==SDLK_F11) {
                if(event.type==SDL_KEYDOWN) { fullscreen=!fullscreen; if(!video(320*scale,240*scale)) {out->type=WOLF3D_EVENT_QUIT;return 1;} }
                break;
            }
            out->key=scan(event.key.keysym.sym);
            if(out->key && focused) { out->type=WOLF3D_EVENT_KEY; out->pressed=event.type==SDL_KEYDOWN;keys[out->key]=(unsigned char)out->pressed;return 1; }
            break;
        case SDL_MOUSEMOTION:
            if(mouse && capture && focused) {out->type=WOLF3D_EVENT_MOUSE_MOTION;out->x=event.motion.xrel;out->y=event.motion.yrel;return 1;} break;
        case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:
            if(mouse && focused && event.button.button<=3 && event.button.button>=1) {
                static const uint8_t buttons[]={0,1,3,2};out->type=WOLF3D_EVENT_MOUSE_BUTTON;
                out->button=buttons[event.button.button];out->pressed=event.type==SDL_MOUSEBUTTONDOWN;return 1;
            } break;
        default:break;
        }
    }
    /* SDL 1.2 backends may dereference subsystem state even with no devices. */
    if(!(SDL_WasInit(SDL_INIT_JOYSTICK)&SDL_INIT_JOYSTICK))return 0;
    SDL_JoystickUpdate();
    for(i=0;i<WOLF3D_MAX_JOYSTICKS;++i) if(joysticks[i]) {
        int16_t x=0,y=0; uint32_t buttons=0; int j;
        if(SDL_JoystickNumAxes(joysticks[i])>0) x=SDL_JoystickGetAxis(joysticks[i],0);
        if(SDL_JoystickNumAxes(joysticks[i])>1) y=SDL_JoystickGetAxis(joysticks[i],1);
        for(j=0;j<4 && j<SDL_JoystickNumButtons(joysticks[i]);++j) if(SDL_JoystickGetButton(joysticks[i],j)) buttons|=1U<<j;
        if(joy_dirty[i] || x!=joy_x[i] || y!=joy_y[i] || buttons!=joy_buttons[i]) {
            joy_dirty[i]=0;joy_x[i]=x;joy_y[i]=y;joy_buttons[i]=buttons;
            out->type=WOLF3D_EVENT_JOYSTICK;out->connected=1;out->joystick=(uint8_t)i;
            out->x=x;out->y=y;out->buttons=buttons;return 1;
        }
    }
    return 0;
}
static void SDLCALL audio_callback(void *unused, Uint8 *stream, int length)
{
    size_t frames=(size_t)length/(pcm_channels*sizeof(int16_t)),first;
    int16_t *dst=(int16_t *)stream;
    (void)unused;
    memset(stream,0,(size_t)length);
    if(pcm_probe_active) {++pcm_probe_calls;pcm_probe_requested+=(unsigned long)frames;}
    if(frames>pcm_count) frames=pcm_count;
    if(pcm_probe_active)pcm_probe_consumed+=(unsigned long)frames;
    first=pcm_capacity-pcm_read;if(first>frames)first=frames;
    memcpy(dst,pcm+pcm_read*pcm_channels,first*pcm_channels*sizeof(*pcm));
    memcpy(dst+first*pcm_channels,pcm,(frames-first)*pcm_channels*sizeof(*pcm));
    pcm_read+=frames;if(pcm_read>=pcm_capacity)pcm_read-=pcm_capacity;
    pcm_count-=frames;
}
static int pcm_init_buffered(const wolf3d_pcm_format_t *request,wolf3d_pcm_format_t *obtained,Uint16 callback_frames)
{
    SDL_AudioSpec want,got;
    pcm_shutdown();
    if(!request || !obtained || request->channels<1 || request->channels>2 || request->bits_per_sample!=16) return 0;
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0) return 0;
    memset(&want,0,sizeof(want));want.freq=(int)request->sample_rate;
    want.channels=(Uint8)request->channels;want.format=AUDIO_S16SYS;want.samples=callback_frames;want.callback=audio_callback;
    if(SDL_OpenAudio(&want,&got)<0) return 0;
    pcm_open=1;
    if(got.format!=AUDIO_S16SYS || got.channels<1 || got.channels>2 || got.freq<=0 || !got.samples) {pcm_shutdown();return 0;}
    pcm_channels=got.channels;pcm_limit=(size_t)got.freq/10;
    pcm_callback_samples=got.samples;
    if(pcm_limit<1024) pcm_limit=1024;
    /* SDL waveOut can replace our 512-frame request with a quarter-second
       callback. Keep two negotiated callbacks ready, not just 100 ms. */
    if(pcm_limit<(size_t)got.samples*2U)pcm_limit=(size_t)got.samples*2U;
    pcm_capacity=pcm_limit>WG_QUEUE_FRAMES?pcm_limit:WG_QUEUE_FRAMES;
    if(pcm_capacity>((size_t)-1)/pcm_channels/sizeof(*pcm)) {pcm_shutdown();return 0;}
    pcm=(int16_t *)malloc(pcm_capacity*pcm_channels*sizeof(*pcm));
    if(!pcm) {pcm_shutdown();return 0;}
    obtained->sample_rate=(uint32_t)got.freq;obtained->channels=got.channels;obtained->bits_per_sample=16;
#ifdef WG_LINUX_OPL
    WG_LinuxOPLSetRate(obtained->sample_rate);
#endif
#ifdef WG_SDL1_OPL_WORKER
    pcm_rate=obtained->sample_rate;
#endif
    {
        char driver[64];
        if(!SDL_AudioDriverName(driver,sizeof(driver)))strcpy(driver,"unknown SDL driver");
        WG_SetAudioOutputInfo(NULL,"sdl",driver);
    }
    SDL_PauseAudio(0);return 1;
}
static int pcm_init_ex(const wolf3d_pcm_format_t *request,wolf3d_pcm_format_t *obtained)
{
    if(pcm_output_choice!=2 && pcm_init_buffered(request,obtained,512))return 1;
#ifdef _WIN32
    if(pcm_output_choice!=1) {
        pcm_shutdown();
        if(request && obtained && request->bits_per_sample==16
            && WG_WaveOutOpen(&pcm_wave,request->sample_rate,request->channels)) {
            pcm_output_wave=1;*obtained=*request;
#ifdef WG_SDL1_OPL_WORKER
            pcm_rate=request->sample_rate;
#endif
            WG_SetAudioOutputInfo(NULL,"waveout","Windows waveOut (4 x 512 frames)");return 1;
        }
    }
#endif
    WG_SetAudioOutputInfo(NULL,"unavailable","PCM initialization failed");return 0;
}
static int pcm_init(uint32_t rate,uint16_t channels)
{
    wolf3d_pcm_format_t request={0,0,16},got;
    request.sample_rate=rate;request.channels=channels;return pcm_init_ex(&request,&got);
}
static size_t writable(void)
{
    size_t result;
#ifdef _WIN32
    if(pcm_output_wave)return WG_WaveOutWritable(&pcm_wave);
#endif
    if(!pcm_open)return 0;
    SDL_LockAudio();result=pcm_limit-pcm_count;SDL_UnlockAudio();return result;
}
static int submit(const int16_t *samples,size_t frames)
{
    size_t write,first;
#ifdef _WIN32
    if(pcm_output_wave) {
        wg_waveout_submit_t publish=NULL;
#ifdef WG_SDL1_OPL_WORKER
        if(pcm_native)publish=WG_OPLPlaybackSubmit;
#endif
        return WG_WaveOutSubmit(&pcm_wave,samples,frames,publish);
    }
#endif
    if(!pcm_open || !samples)return 0;
    SDL_LockAudio();
    if(frames>pcm_limit-pcm_count) {SDL_UnlockAudio();return 0;}
    write=pcm_read+pcm_count;if(write>=pcm_capacity)write-=pcm_capacity;
    first=pcm_capacity-write;if(first>frames)first=frames;
    memcpy(pcm+write*pcm_channels,samples,first*pcm_channels*sizeof(*pcm));
    memcpy(pcm,samples+first*pcm_channels,(frames-first)*pcm_channels*sizeof(*pcm));
    pcm_count+=frames;
#ifdef WG_LINUX_OPL
    /* Publish before unlocking: the callback must not drain unannounced PCM. */
    if (!WG_LinuxOPLSubmit(frames, pcm_count)) {
        SDL_UnlockAudio();return 0;
    }
#endif
#ifdef WG_SDL1_OPL_WORKER
    if(!WG_OPLPlaybackSubmitClockedFrames(frames)) {SDL_UnlockAudio();return 0;}
#endif
    SDL_UnlockAudio();return 1;
}
static int pcm_probe_buffer(char *report,size_t capacity,Uint16 callback_frames)
{
    wolf3d_pcm_format_t request={22050,2,16},got;
    char driver[64];
    uint32_t start,elapsed,sdl_start,sdl_elapsed;
    unsigned long calls,requested,consumed,polls=0;
    size_t used=strlen(report);
    if(!pcm_init_buffered(&request,&got,callback_frames)) {
        snprintf(report+used,capacity-used,"  SDL PCM probe: initialization failed: %s\n",SDL_GetError());return -1;
    }
    if(!SDL_AudioDriverName(driver,sizeof(driver)))strcpy(driver,"unknown");
    SDL_LockAudio();
    memset(pcm,0,pcm_capacity*pcm_channels*sizeof(*pcm));pcm_read=0;pcm_count=pcm_limit;
    pcm_probe_calls=pcm_probe_requested=pcm_probe_consumed=0;pcm_probe_active=1;
    SDL_UnlockAudio();
    sdl_start=SDL_GetTicks();
#ifdef _WIN32
    start=GetTickCount();
#else
    start=sdl_start;
#endif
    do {
        SDL_Delay(1);
        ++polls;
        SDL_LockAudio();pcm_count=pcm_limit;SDL_UnlockAudio();
#ifdef _WIN32
        elapsed=GetTickCount()-start;
#else
        elapsed=SDL_GetTicks()-start;
#endif
    } while(elapsed<2000U);
    sdl_elapsed=SDL_GetTicks()-sdl_start;
    SDL_LockAudio();pcm_probe_active=0;
    calls=pcm_probe_calls;requested=pcm_probe_requested;consumed=pcm_probe_consumed;
    SDL_UnlockAudio();
    snprintf(report+used,capacity-used,
        "  SDL PCM driver: %s\n  PCM probe: %lu Hz, %u channels, 16-bit; %u frames/callback\n"
        "  Silent probe: %lu ms wall, %lu ms SDL; %lu callbacks; %lu refill polls\n"
        "  PCM frames requested/consumed: %lu/%lu (%lu frames/sec; %lu underrun frames)\n",
        driver,(unsigned long)got.sample_rate,(unsigned)got.channels,pcm_callback_samples,
        (unsigned long)elapsed,(unsigned long)sdl_elapsed,calls,polls,requested,consumed,
        requested*1000UL/elapsed,requested-consumed);
    pcm_shutdown();
    return !strcmp(driver,"dsound");
}
static void pcm_diagnostics(char *report,size_t capacity)
{
    int probe=0;
#ifdef _WIN32
    if(pcm_output_choice!=2)probe=pcm_probe_buffer(report,capacity,512);
    if(pcm_output_choice==2 || (probe<0 && pcm_output_choice==0)) {
        wolf3d_pcm_format_t request={22050,2,16},got;
        size_t used=strlen(report);
        int ok=pcm_init_ex(&request,&got);
        snprintf(report+used,capacity-used,"  Shared waveOut probe: %s; 4 x 512 frames at 22050 Hz\n",ok?"initialized":"failed");
        pcm_shutdown();return;
    }
#else
    probe=pcm_probe_buffer(report,capacity,512);
#endif
    if(probe==1) {
        /* Compare fragment sizes without changing the gameplay default. */
        (void)pcm_probe_buffer(report,capacity,1024);
        (void)pcm_probe_buffer(report,capacity,2048);
        (void)pcm_probe_buffer(report,capacity,4096);
    }
}
#ifdef _WIN32
#include "WG_SDL1_WINDOWS.h"
#endif
#ifdef WG_SDL1_DYNAMIC
#include "../win32/WG_BACKEND.h"
static int backend_init_failed;
static int backend_init(void)
{
    int ok=ui_init();backend_init_failed=!ok;return ok;
}
#endif
int WG_InstallPlatform(void)
{
    const wolf3d_platform_api_t api={WOLF3D_PLATFORM_API_VERSION,sizeof(api),
#ifdef _WIN32
#ifdef WG_SDL1_DYNAMIC
        backend_init,
#else
        ui_init,
#endif
        ui_shutdown,ui_present,ticks,sleep_ms,ui_poll,interactive,ui_set_title,message,error,text_screen,
#else
        init,shutdown_host,present,ticks,sleep_ms,poll_event,interactive,title,message,error,text_screen,
#endif
        pcm_init,pcm_shutdown,writable,submit,pcm_init_ex,
#ifdef WG_LINUX_OPL
        WG_LinuxOPLInit,WG_LinuxOPLShutdown,WG_LinuxOPLWrite,
#elif defined(WG_SDL1_OPL_WORKER)
        opl_init,opl_shutdown,WG_DOSAdLibWrite,
#else
        NULL,NULL,NULL,
#endif
#ifdef _WIN32
        ui_get_devices,ui_set_capture,
#else
        devices,mouse_capture,
#endif
#ifdef WG_LINUX_OPL
        WG_LinuxOPLWriteTimed
#elif defined(WG_SDL1_OPL_WORKER)
        WG_OPLPlaybackWrite
#else
        NULL
#endif
    };
    return wolf3d_SetPlatform(&api)==WOLF3D_RESULT_OK;
}
#ifndef WG_SDL1_TEST
static int run_main(int argc,char **argv)
{
    wg_launcher_arguments_t launcher;wg_game_arguments_t game;
    char problem[2048];int i,ok;wolf3d_result_t result;
    if(!WG_LoadLauncherArguments(argc,argv,&launcher,problem,sizeof(problem))) {WG_PrintLauncherError(problem);return 1;}
    argc=launcher.argc;argv=launcher.argv;
#ifdef _WIN32
    pcm_output_choice=WG_ParseAudioOutput(argc,argv,1,1);
#else
    pcm_output_choice=WG_ParseAudioOutput(argc,argv,1,0);
#endif
    if(pcm_output_choice<0) {WG_FreeLauncherArguments(&launcher);return 1;}
#ifndef WG_SDL1_DYNAMIC
    if(!WG_ValidateSingleBackend(argc,argv,"sdl1")) {WG_FreeLauncherArguments(&launcher);return 1;}
#endif
    if(WG_CommandLineHelpRequested(argc,argv)) {
        WG_SetVideoInfo("sdl1","sdl1",NULL);
        WG_PrintCommandLineHelp(argv[0],"SDL 1.2 options:\n  --window-scale N     Initial 4:3 window scale (1-4; default 2)\n  --fullscreen         Start in the selected fullscreen mode\n  --windowed           Windowed mode (default)\n",WG_FULLSCREEN_HELP "F11 toggles fullscreen.\n" WG_SDL1_VIDEO_HELP "  --sdl-render auto|opengl|surface  Presentation (default auto)\n  --gl-acceleration auto|hardware|software  Require OpenGL with this acceleration\n  --gl-library FILE     Load this OpenGL implementation (implies opengl)\n" WG_SDL1_OPL_NOTE "\n");
        WG_FreeLauncherArguments(&launcher);return 0;
    }
    for(i=1;i<argc;++i) {
        int option=WG_FullscreenOption(&fullscreen_options,argv[i],i+1<argc?argv[i+1]:NULL);
        if(option<0) {error(WG_FULLSCREEN_ERROR);WG_FreeLauncherArguments(&launcher);return 1;}
        if(option) {++i;continue;}
        if(!strcmp(argv[i],"--gl-acceleration")) {
            if(++i>=argc || !set_gl_acceleration(argv[i])) {error("--gl-acceleration requires auto, hardware or software");WG_FreeLauncherArguments(&launcher);return 1;}
            render_choice=1;continue;
        }
        if(!strcmp(argv[i],"--gl-library")) {
            if(++i>=argc || !argv[i][0]) {error("--gl-library requires a DLL/shared-library path");WG_FreeLauncherArguments(&launcher);return 1;}
            if(strlen(argv[i])>=sizeof(gl_library_storage)) {error("OpenGL library path is too long");WG_FreeLauncherArguments(&launcher);return 1;}
            strcpy(gl_library_storage,argv[i]);gl_library_path=gl_library_storage;render_choice=1;continue;
        }
        if(!strcmp(argv[i],"--sdl-render")) {
            if(++i>=argc || !set_renderer(argv[i])) {error("--sdl-render requires auto, opengl or surface");WG_FreeLauncherArguments(&launcher);return 1;}
        }
#ifdef _WIN32
        if(!strcmp(argv[i],"--sdl-video")) {
            if(++i>=argc || !set_video_driver(argv[i])) {error("--sdl-video requires directx or windib");WG_FreeLauncherArguments(&launcher);return 1;}
        }
        if(!strcmp(argv[i],"--sdl-scale")) {
            if(++i>=argc || !set_scaling(argv[i])) {error("--sdl-scale requires hardware or software");WG_FreeLauncherArguments(&launcher);return 1;}
        }
#endif
        if(!strcmp(argv[i],"--fullscreen"))fullscreen=1;
        if(!strcmp(argv[i],"--windowed"))fullscreen=0;
        if(!strcmp(argv[i],"--nomouse"))mouse=0;
        if(!strcmp(argv[i],"--mouse"))mouse=1;
        if(!strcmp(argv[i],"--nojoy"))joy=0;
        if(!strcmp(argv[i],"--joy"))joy=1;
        if(!strcmp(argv[i],"--window-scale")) {
            if(++i>=argc || strlen(argv[i])!=1 || argv[i][0]<'1' || argv[i][0]>'4') {error("--window-scale requires 1, 2, 3 or 4");WG_FreeLauncherArguments(&launcher);return 1;}
            scale=argv[i][0]-'0';
        }
    }
    if((gl_library_path || gl_acceleration>=0) && render_choice!=1) {
        error("--gl-library and --gl-acceleration require --sdl-render opengl");WG_FreeLauncherArguments(&launcher);return 1;
    }
    if(!WG_InstallPlatform()) {WG_PrintLauncherError("SDL1: engine rejected the platform interface. Replace the complete package, including wolf3d.dll.\n");WG_FreeLauncherArguments(&launcher);return 1;}
    sdl_diagnostics=WG_CommandLineDiagnosticsRequested(argc,argv);
    if(sdl_diagnostics) {
        char hardware_report[4096]="  SDL 1.2 host: platform SDL video and PCM\n  " WG_SDL1_OPL_NOTE "\n";
#ifdef _WIN32
        ok=ui_init();
#else
        ok=init();
#endif
        if(!ok) {HOST_SHUTDOWN();WG_FreeLauncherArguments(&launcher);return 1;}
        pcm_diagnostics(hardware_report,sizeof(hardware_report));
        WG_SetVideoInfo("sdl1","sdl1",sdl_report);
        ok=WG_PrintDiagnostics(argc,argv,launcher.config_path,hardware_report);
        HOST_SHUTDOWN();WG_FreeLauncherArguments(&launcher);return ok?0:1;
    }
    if(!WG_PrepareGameArguments(argc,argv,&game,problem,sizeof(problem))) {WG_PrintLauncherError(problem);WG_FreeLauncherArguments(&launcher);return 1;}
    result=wolf3d_Create(game.argc,game.argv);WG_FreeGameArguments(&game);WG_FreeLauncherArguments(&launcher);
    if(result==WOLF3D_RESULT_OK) {result=wolf3d_Run();wolf3d_Shutdown();}
    HOST_SHUTDOWN();
    return result==WOLF3D_RESULT_QUIT || result==WOLF3D_RESULT_NOT_IMPLEMENTED ? 0:1;
}
#ifdef WG_SDL1_DYNAMIC
int WG_SDL1Run(int argc,char **argv)
{
    int result;backend_init_failed=0;
    SDL_SetModuleHandle(GetModuleHandle(NULL));
    result=run_main(argc,argv);
    return backend_init_failed && render_choice!=1?WG_BACKEND_UNAVAILABLE:result;
}
#else
#ifdef _WIN32
#include "../win32/WG_WIN32_CONSOLE.h"
#endif
int main(int argc,char **argv)
{
    int result;
#ifdef _WIN32
    int owned=WG_OwnConsole();
    SDL_SetModuleHandle(GetModuleHandle(NULL));
#endif
    result=run_main(argc,argv);
#ifdef _WIN32
    WG_PauseConsole(owned,WG_NoPause(argc,argv));
#endif
    return result;
}
#endif /* standalone entry point */
#endif
