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
static void ui_shutdown(void);
#define HOST_SHUTDOWN ui_shutdown
#else
#define HOST_SHUTDOWN shutdown_host
#endif
#ifdef WG_INPOUT32
#include "../win32/WG_INPOUT32.h"
#include "../win32/WG_OPL_PLAYBACK.h"
#include "../dos/WG_DOS_ADLIB.h"
static unsigned pcm_rate;
static void opl_shutdown(void) {WG_OPLPlaybackStop();WG_DOSAdLibShutdown();}
static int opl_init(void) {
    if(!WG_DOSAdLibInit())return 0;
    if(WG_OPLPlaybackStart(NULL,pcm_rate))return 1;
    WG_DOSAdLibShutdown();return 0;
}
#endif
#ifdef WG_WIN9X
#include "../dos/WG_DOS_ADLIB.h"
#endif
#ifdef WG_LINUX_OPL
#include "../linux/WG_LINUX_OPL.h"
#define WG_SDL1_OPL_NOTE "Native OPL: direct ports 0x388-0x38b; RH9 elapsed-time waits, hardware confirmation pending"
#elif defined(WG_INPOUT32)
#define WG_SDL1_OPL_NOTE "Native OPL: InpOut32, timestamped playback"
#elif defined(WG_WIN9X)
#define WG_SDL1_OPL_NOTE "Native OPL: direct ISA ports on Windows 95/98/Me"
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
static int16_t pcm[WG_QUEUE_FRAMES * 2U];
static size_t pcm_read, pcm_count, pcm_limit;
static unsigned pcm_channels;
static int pcm_open;
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
#ifdef WG_INPOUT32
    opl_shutdown();pcm_rate=0;
#endif
#ifdef WG_LINUX_OPL
    WG_LinuxOPLShutdown();
    WG_LinuxOPLSetRate(0);
#endif
    if (pcm_open) SDL_CloseAudio();
    pcm_open = 0; pcm_count = pcm_read = 0;
}
static void shutdown_host(void)
{
    unsigned i;
    pcm_shutdown();
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
    while(SDL_PollEvent(&event)) {
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
    size_t frames=(size_t)length/(pcm_channels*sizeof(int16_t)),i,c;
    int16_t *dst=(int16_t *)stream;
    (void)unused;
    memset(stream,0,(size_t)length);
    if(frames>pcm_count) frames=pcm_count;
    for(i=0;i<frames;++i) for(c=0;c<pcm_channels;++c)
        dst[i*pcm_channels+c]=pcm[((pcm_read+i)%WG_QUEUE_FRAMES)*pcm_channels+c];
    pcm_read=(pcm_read+frames)%WG_QUEUE_FRAMES;pcm_count-=frames;
}
static int pcm_init_ex(const wolf3d_pcm_format_t *request,wolf3d_pcm_format_t *obtained)
{
    SDL_AudioSpec want,got;
    pcm_shutdown();
    if(!request || !obtained || request->channels<1 || request->channels>2 || request->bits_per_sample!=16) return 0;
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0) return 0;
    memset(&want,0,sizeof(want));want.freq=(int)request->sample_rate;
    want.channels=(Uint8)request->channels;want.format=AUDIO_S16SYS;want.samples=512;want.callback=audio_callback;
    if(SDL_OpenAudio(&want,&got)<0) {error(SDL_GetError());return 0;}
    pcm_open=1;
    if(got.format!=AUDIO_S16SYS || got.channels<1 || got.channels>2 || got.freq<=0) {pcm_shutdown();return 0;}
    pcm_channels=got.channels;pcm_limit=(size_t)got.freq/10;
    if(pcm_limit<1024) pcm_limit=1024;
    if(pcm_limit>WG_QUEUE_FRAMES) pcm_limit=WG_QUEUE_FRAMES;
    obtained->sample_rate=(uint32_t)got.freq;obtained->channels=got.channels;obtained->bits_per_sample=16;
#ifdef WG_LINUX_OPL
    WG_LinuxOPLSetRate(obtained->sample_rate);
#endif
#ifdef WG_INPOUT32
    pcm_rate=obtained->sample_rate;
#endif
    SDL_PauseAudio(0);return 1;
}
static int pcm_init(uint32_t rate,uint16_t channels)
{
    wolf3d_pcm_format_t request={0,0,16},got;
    request.sample_rate=rate;request.channels=channels;return pcm_init_ex(&request,&got);
}
static size_t writable(void)
{
    size_t result;if(!pcm_open)return 0;
    SDL_LockAudio();result=pcm_limit-pcm_count;SDL_UnlockAudio();return result;
}
static int submit(const int16_t *samples,size_t frames)
{
    size_t i,c;if(!pcm_open || !samples)return 0;
    SDL_LockAudio();
    if(frames>pcm_limit-pcm_count) {SDL_UnlockAudio();return 0;}
    for(i=0;i<frames;++i) for(c=0;c<pcm_channels;++c)
        pcm[((pcm_read+pcm_count+i)%WG_QUEUE_FRAMES)*pcm_channels+c]=samples[i*pcm_channels+c];
    pcm_count+=frames;
#ifdef WG_LINUX_OPL
    /* Publish before unlocking: the callback must not drain unannounced PCM. */
    if (!WG_LinuxOPLSubmit(frames, pcm_count)) {
        SDL_UnlockAudio();return 0;
    }
#endif
#ifdef WG_INPOUT32
    if(!WG_OPLPlaybackSubmitFrames(frames,pcm_count)) {SDL_UnlockAudio();return 0;}
#endif
    SDL_UnlockAudio();return 1;
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
#elif defined(WG_INPOUT32)
        opl_init,opl_shutdown,WG_DOSAdLibWrite,
#elif defined(WG_WIN9X)
        WG_DOSAdLibInit,WG_DOSAdLibShutdown,WG_DOSAdLibWrite,
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
#elif defined(WG_INPOUT32)
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
#ifdef _WIN32
        ok=ui_init();
#else
        ok=init();
#endif
        if(!ok) {HOST_SHUTDOWN();WG_FreeLauncherArguments(&launcher);return 1;}
        WG_SetVideoInfo("sdl1","sdl1",sdl_report);
        ok=WG_PrintDiagnostics(argc,argv,launcher.config_path,"  SDL 1.2 host: platform SDL video and PCM\n  " WG_SDL1_OPL_NOTE "\n");
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
