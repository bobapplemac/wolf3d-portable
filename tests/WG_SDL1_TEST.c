#define WG_SDL1_TEST
#undef NDEBUG
#include <assert.h>
#ifdef WG_SDL1_OPL_MOCK
#include "../platforms/linux/WG_LINUX_OPL.h"
static uint32_t mock_rate;
static int mock_active, mock_fail, mock_submits;
static size_t mock_frames;
static uint64_t mock_queued;
int WG_LinuxOPLActive(void) { return mock_active; }
void WG_LinuxOPLSetRate(uint32_t rate) { mock_rate=rate; }
int WG_LinuxOPLInit(void) { return mock_active=mock_rate!=0; }
void WG_LinuxOPLShutdown(void) { mock_active=0; }
void WG_LinuxOPLWrite(uint16_t reg,uint8_t value) { (void)reg;(void)value; }
void WG_LinuxOPLWriteTimed(uint16_t reg,uint8_t value,uint64_t frame,uint32_t rate)
{ (void)reg;(void)value;(void)frame;(void)rate; }
int WG_LinuxOPLSubmit(size_t frames,uint64_t queued)
{ if(!mock_active)return 1; ++mock_submits;mock_frames=frames;mock_queued=queued;return !mock_fail; }
#endif
#ifdef _WIN32
#define WG_WriteWindowsTextScreen test_windows_text
#endif
#include <SDL/SDL.h>
#ifdef WG_SDL1_DYNAMIC
#include "../platforms/sdl1/WG_SDL1_DYNAMIC.h"
static int (SDLCALL *test_push_event)(SDL_Event *);
#define SDL_PushEvent test_push_event
#endif
static int joystick_updates;
static void checked_joystick_update(void)
{
    assert(SDL_WasInit(SDL_INIT_JOYSTICK)&SDL_INIT_JOYSTICK);
    ++joystick_updates;SDL_JoystickUpdate();
}
#undef SDL_JoystickUpdate
#define SDL_JoystickUpdate checked_joystick_update
#include <SDL/SDL_opengl.h>
static int gl_checks;
static void checked_gl_swap(void)
{
    void (APIENTRY *read_pixels)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid *);
    void *address=SDL_GL_GetProcAddress("glReadPixels");
    unsigned char rgb[4]={0};
    assert(address);memcpy(&read_pixels,&address,sizeof(read_pixels));
    /* Test window is 800x480: black pillarbox, red top, blue bottom. */
    read_pixels(10,240,1,1,GL_RGB,GL_UNSIGNED_BYTE,rgb);
    assert(rgb[0]==0 && rgb[1]==0 && rgb[2]==0);
    read_pixels(400,400,1,1,GL_RGB,GL_UNSIGNED_BYTE,rgb);
    assert(rgb[0]==255 && rgb[1]==0 && rgb[2]==0);
    read_pixels(400,40,1,1,GL_RGB,GL_UNSIGNED_BYTE,rgb);
    assert(rgb[0]==0 && rgb[1]==0 && rgb[2]==255);
    ++gl_checks;SDL_GL_SwapBuffers();
}
#undef SDL_GL_SwapBuffers
#define SDL_GL_SwapBuffers checked_gl_swap
#include "../platforms/sdl1/WG_SDL1.c"
#ifdef _WIN32
static int native_text_calls,driver_attempts,accept_directx,accept_windib;
static int fake_start_driver(const char *driver)
{
    ++driver_attempts;
    if(!strcmp(driver,"directx"))return accept_directx;
    assert(!strcmp(driver,"windib"));return accept_windib;
}
void test_windows_text(const uint8_t *cells,uint16_t columns,uint16_t rows)
{
    assert(!ui_thread); /* Window/video must be shut down before the exit card. */
    assert(columns==1 && rows==1 && cells[0]==0xdb && cells[1]==0x4f);
    ++native_text_calls;
}
#endif
int main(void)
{
    wolf3d_pcm_format_t request={22050,2,16},got;
    int16_t samples[16],output[24];
    wolf3d_event_t out; SDL_Event input;
    uint8_t pixels[320*200],palette[768];
    int i;
    assert(set_renderer("auto") && render_choice==0);
    assert(set_renderer("opengl") && render_choice==1);
    assert(set_renderer("surface") && render_choice==2);
    assert(!set_renderer("invalid") && render_choice==2);
    assert(gl_software_renderer("GDI Generic"));
    assert(gl_software_renderer("llvmpipe"));
    assert(!gl_software_renderer("GeForce FX 5900XT"));
    render_choice=0;
#ifdef WG_SDL1_DYNAMIC
    {
        char reason[256];FARPROC address;
        assert(WG_SDL1Load(reason,sizeof(reason)));
        address=GetProcAddress(GetModuleHandleA("SDL.dll"),"SDL_PushEvent");
        assert(address);memcpy(&test_push_event,&address,sizeof(test_push_event));
        SDL_SetModuleHandle(GetModuleHandle(NULL));
    }
#endif
    if(getenv("WG_TEST_GL")) {
        GLint filtering=0;
        void (APIENTRY *get_parameter)(GLenum,GLenum,GLint *);
        void *address;
        joy=0;render_choice=1;assert(init() && gl_active);
        memset(pixels,1,sizeof(pixels)/2);
        memset(pixels+sizeof(pixels)/2,2,sizeof(pixels)/2);
        memset(palette,0,sizeof(palette));palette[3]=255;palette[8]=255;
        for(i=0;i<3;++i) {
            assert(video(800,480) && gl_active);present(pixels,palette);
            address=SDL_GL_GetProcAddress("glGetTexParameteriv");assert(address);
            memcpy(&get_parameter,&address,sizeof(get_parameter));
            get_parameter(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&filtering);
            assert(filtering==GL_NEAREST);
            assert(wg_glGetError()==GL_NO_ERROR);
        }
        assert(gl_checks==3);shutdown_host();assert(!gl_active && !gl_texture);
#ifdef WG_SDL1_DYNAMIC
        WG_SDL1Unload();
#endif
        puts("SDL1 OpenGL: pixel colors, orientation, 4:3 borders, nearest filtering, context recreation and shutdown passed");
        return 0;
    }
    assert(scan(SDLK_a)==0x1e && scan(SDLK_KP8)==WOLF3D_KEY_UP);
    assert(scan(SDLK_UNKNOWN)==0);
#ifdef _WIN32
    assert(set_video_driver("directx") && !strcmp(requested_video_driver(),"directx"));
    assert(set_video_driver("windib") && !strcmp(requested_video_driver(),"windib"));
    assert(!set_video_driver("invalid"));video_driver_choice=NULL;
    assert(!dd_software_requested && set_scaling("software") && dd_software_requested);
    assert(set_scaling("hardware") && !dd_software_requested && !set_scaling("invalid"));
    accept_directx=1;assert(start_video_with_fallback("directx",fake_start_driver) && driver_attempts==1);
    driver_attempts=0;accept_directx=0;accept_windib=1;
    assert(start_video_with_fallback("directx",fake_start_driver) && driver_attempts==2);
    driver_attempts=0;accept_windib=0;
    assert(!start_video_with_fallback("directx",fake_start_driver) && driver_attempts==2);
    driver_attempts=0;assert(!start_video_with_fallback("windib",fake_start_driver) && driver_attempts==1);
#endif
    assert(WG_FullscreenOption(&fullscreen_options,"--fullscreen-mode","borderless")==1);
    assert(!WG_FullscreenDisplay(&fullscreen_options));
    joy=0;assert(init());
    assert(!(SDL_WasInit(SDL_INIT_JOYSTICK)&SDL_INIT_JOYSTICK));
    while(poll_event(&out)) {}
    assert(!joystick_updates && !(devices()&WOLF3D_INPUT_DEVICE_JOYSTICK));
#ifdef _WIN32
    fullscreen=1;assert(video(640,480));
    assert(!(screen->flags&SDL_FULLSCREEN) && (screen->flags&SDL_NOFRAME));
    assert(screen->w==GetSystemMetrics(SM_CXSCREEN) && screen->h==GetSystemMetrics(SM_CYSCREEN));
    fullscreen=0;assert(video(640,480));assert(!(screen->flags&SDL_FULLSCREEN));
#endif
    fullscreen=1;mouse=0;mouse_capture(0);assert(SDL_ShowCursor(SDL_QUERY)==SDL_DISABLE);
    focused=0;mouse_capture(0);assert(SDL_ShowCursor(SDL_QUERY)==SDL_ENABLE);
    focused=1;mouse_capture(0);assert(SDL_ShowCursor(SDL_QUERY)==SDL_DISABLE);
    fullscreen=0;mouse=1;mouse_capture(0);assert(SDL_ShowCursor(SDL_QUERY)==SDL_ENABLE);
    mouse_capture(1);assert(SDL_ShowCursor(SDL_QUERY)==SDL_DISABLE);mouse_capture(0);
    memset(pixels,1,sizeof(pixels));memset(palette,0,sizeof(palette));palette[3]=255;
    present(pixels,palette);
    assert(screen->w==640 && screen->h==480 && screen->format->BytesPerPixel==4);
    assert(*(Uint32 *)screen->pixels==SDL_MapRGB(screen->format,255,0,0));
    assert(video(800,480));present(pixels,palette);
    assert(*(Uint32 *)screen->pixels==0); /* Pillarbox, never stretched. */
    {
        SDL_Surface *cached=software_frame;
        palette[3]=0;palette[5]=255;present(pixels,palette);
        assert(software_frame==cached && *(Uint32 *)screen->pixels==0);
        assert(*(Uint32 *)((Uint8 *)screen->pixels+80*4)==SDL_MapRGB(screen->format,0,0,255));
        for(i=0;i<320*200;++i)pixels[i]=(uint8_t)(i%251);
        for(i=0;i<768;++i)palette[i]=(uint8_t)i;
        assert(video(323,243));present(pixels,palette);
        assert(software_frame->w==320 && software_frame->h==200);
        assert(*(Uint32 *)screen->pixels==SDL_MapRGB(screen->format,palette[0],palette[1],palette[2]));
    }
    memset(&input,0,sizeof(input));input.type=SDL_KEYDOWN;input.key.keysym.sym=SDLK_ESCAPE;
    assert(SDL_PushEvent(&input)==0);assert(poll_event(&out));
    assert(out.type==WOLF3D_EVENT_KEY && out.key==WOLF3D_KEY_ESCAPE && out.pressed);
    focused=0;assert(poll_event(&out));assert(!out.pressed && out.key==WOLF3D_KEY_ESCAPE);focused=1;
    assert(pcm_init_ex(&request,&got));SDL_PauseAudio(1);
    assert(got.sample_rate==22050 && got.channels==2);
#ifdef WG_SDL1_OPL_MOCK
    assert(mock_rate==got.sample_rate && WG_LinuxOPLInit());
#endif
    for(i=0;i<16;++i)samples[i]=(int16_t)(i+1);
    assert(submit(samples,8));assert(pcm_count==8);
#ifdef WG_SDL1_OPL_MOCK
    assert(mock_submits==1 && mock_frames==8 && mock_queued==8);
#endif
    audio_callback(NULL,(Uint8 *)output,sizeof(output));
    for(i=0;i<16;++i)assert(output[i]==samples[i]);
    for(i=16;i<24;++i)assert(output[i]==0); /* Underrun is silence. */
    assert(pcm_count==0);
    pcm_read=WG_QUEUE_FRAMES-2;assert(submit(samples,8));
    audio_callback(NULL,(Uint8 *)output,32);
    assert(!memcmp(samples,output,32)); /* Wrap preserves interleaved channels. */
    assert(!submit(samples,pcm_limit+1));
#ifdef WG_SDL1_OPL_MOCK
    assert(mock_submits==2); /* Overflow must not publish OPL time. */
    mock_fail=1;assert(!submit(samples,8)); /* Worker errors reach the engine. */
#endif
    pcm_shutdown();assert(!writable());
#ifdef WG_SDL1_OPL_MOCK
    assert(!mock_active && !mock_rate);
#endif
    assert(pcm_init_ex(&request,&got));pcm_shutdown();
    shutdown_host();
    joy=1;assert(init());assert(SDL_WasInit(SDL_INIT_JOYSTICK)&SDL_INIT_JOYSTICK);
    while(poll_event(&out)) {}
    assert(joystick_updates>0);shutdown_host();joy=0;
#ifdef _WIN32
    /* Also exercise the real SDL lifecycle on the isolated window thread. */
    assert(ui_init());ui_present(pixels,palette);ui_set_capture(0);
    ui_set_title("SDL1 worker lifecycle test");SDL_Delay(30);
    { const uint8_t cells[]={0xdb,0x4f};text_screen(cells,1,1);assert(native_text_calls==1); }
    ui_shutdown();
#endif
#ifdef WG_SDL1_DYNAMIC
    WG_SDL1Unload();
#endif
    puts("SDL1: input, aspect ratio, PCM order/wrap/underrun/overflow and restart passed.");
    return 0;
}
