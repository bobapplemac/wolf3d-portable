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
#include "../platforms/sdl1/WG_SDL1.c"
int main(void)
{
    wolf3d_pcm_format_t request={22050,2,16},got;
    int16_t samples[16],output[24];
    wolf3d_event_t out; SDL_Event input;
    uint8_t pixels[320*200],palette[768];
    int i;
    assert(scan(SDLK_a)==0x1e && scan(SDLK_KP8)==WOLF3D_KEY_UP);
    assert(scan(SDLK_UNKNOWN)==0);
    assert(init());
    memset(pixels,1,sizeof(pixels));memset(palette,0,sizeof(palette));palette[3]=255;
    present(pixels,palette);
    assert(screen->w==640 && screen->h==480 && screen->format->BytesPerPixel==4);
    assert(*(Uint32 *)screen->pixels==SDL_MapRGB(screen->format,255,0,0));
    assert(video(800,480));present(pixels,palette);
    assert(*(Uint32 *)screen->pixels==0); /* Pillarbox, never stretched. */
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
    puts("SDL1: input, aspect ratio, PCM order/wrap/underrun/overflow and restart passed.");
    return 0;
}
