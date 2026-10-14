#define WG_SDL1_TEST
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#define DIRECTDRAW_VERSION 0x0300
#include <ddraw.h>
#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#undef main
static DWORD buffer[200][324];
static int released,blits,fail_blit,fail_lock;
static HRESULT fake_lock(DDSURFACEDESC *desc)
{
    if(fail_lock)return DDERR_SURFACELOST;
    desc->lpSurface=buffer;desc->lPitch=sizeof(buffer[0]);return DD_OK;
}
static HRESULT fake_blt(RECT *dst,LPDIRECTDRAWSURFACE src,RECT *rect)
{
    assert(dst->left==100 && dst->top==200 && dst->right==740 && dst->bottom==680);
    assert(src && rect->left==0 && rect->top==0 && rect->right==320 && rect->bottom==200);
    assert(buffer[0][0]==0x123456 && buffer[199][319]==0x123456 && buffer[0][320]==0xdeadbeef);
    ++blits;return fail_blit?DDERR_NOSTRETCHHW:DD_OK;
}
static BOOL fake_origin(POINT *point) {point->x=100;point->y=200;return TRUE;}
static BOOL fake_rect(RECT *rect) {rect->left=rect->top=0;rect->right=640;rect->bottom=480;return TRUE;}
#undef IDirectDrawSurface_Lock
#define IDirectDrawSurface_Lock(s,r,d,f,h) fake_lock(d)
#undef IDirectDrawSurface_Unlock
#define IDirectDrawSurface_Unlock(s,p) DD_OK
#undef IDirectDrawSurface_Blt
#define IDirectDrawSurface_Blt(s,d,src,r,f,fx) fake_blt(d,src,r)
#undef IDirectDrawSurface_Release
#define IDirectDrawSurface_Release(s) (++released)
#undef IDirectDraw_Release
#define IDirectDraw_Release(s) (++released)
#undef IDirectDrawClipper_Release
#define IDirectDrawClipper_Release(s) (++released)
#define ClientToScreen(w,p) fake_origin(p)
#define GetClientRect(w,r) fake_rect(r)
#include "../platforms/sdl1/WG_SDL1_DDRAW.h"
static void setup(void)
{
    dd_device=(LPDIRECTDRAW)1;dd_primary=(LPDIRECTDRAWSURFACE)2;dd_frame=(LPDIRECTDRAWSURFACE)3;
    dd_disabled=0;dd_format.dwRGBBitCount=32;dd_format.dwRBitMask=0xff0000;dd_format.dwGBitMask=0xff00;dd_format.dwBBitMask=0xff;
}
int main(void)
{
    uint8_t pixels[64000],palette[768];uint16_t packed[200][648];unsigned x,y;
    memset(pixels,1,sizeof(pixels));memset(palette,0,sizeof(palette));palette[3]=0x12;palette[4]=0x34;palette[5]=0x56;
    for(y=0;y<200;++y)for(x=0;x<324;++x)buffer[y][x]=0xdeadbeef;
    setup();assert(dd_present(pixels,palette,0,0,640,480));assert(blits==1 && !dd_disabled);
    fail_blit=1;assert(!dd_present(pixels,palette,0,0,640,480));assert(dd_disabled && !dd_device && released==3);
    assert(!dd_present(pixels,palette,0,0,640,480));assert(blits==2);
    setup();fail_lock=1;assert(!dd_present(pixels,palette,0,0,640,480));assert(dd_disabled && blits==2);
    dd_format.dwRGBBitCount=16;dd_format.dwRBitMask=0xf800;dd_format.dwGBitMask=0x7e0;dd_format.dwBBitMask=31;
    palette[3]=palette[4]=palette[5]=255;dd_upload(packed,sizeof(packed[0]),pixels,palette);assert(packed[0][0]==0xffff);
    dd_format.dwRBitMask=0x7c00;dd_format.dwGBitMask=0x3e0;dd_upload(packed,sizeof(packed[0]),pixels,palette);assert(packed[0][0]==0x7fff);
    dd_format.dwRGBBitCount=24;dd_format.dwRBitMask=0xff0000;dd_format.dwGBitMask=0xff00;dd_format.dwBBitMask=0xff;
    palette[3]=0x12;palette[4]=0x34;palette[5]=0x56;dd_upload(buffer,sizeof(buffer[0]),pixels,palette);
    assert(((uint8_t *)buffer)[0]==0x56 && ((uint8_t *)buffer)[1]==0x34 && ((uint8_t *)buffer)[2]==0x12);
    puts("DirectDraw: native-size upload, pitch, RGB formats, stretch rectangle, blit failure and surface-loss fallback passed");return 0;
}
