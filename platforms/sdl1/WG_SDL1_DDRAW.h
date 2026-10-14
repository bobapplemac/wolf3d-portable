/* All calls run on the SDL window thread. No SDL private surface layout is used. */
#ifndef WG_SDL1_DDRAW_H
#define WG_SDL1_DDRAW_H
#ifndef DIRECTDRAW_VERSION
#define DIRECTDRAW_VERSION 0x0300
#endif
#include <ddraw.h>
#include <SDL/SDL_syswm.h>
static HMODULE dd_library;
static LPDIRECTDRAW dd_device;
static LPDIRECTDRAWSURFACE dd_primary,dd_frame;
static LPDIRECTDRAWCLIPPER dd_clipper;
static HWND dd_window;
static DDPIXELFORMAT dd_format;
static int dd_disabled,dd_announced;
static const char *dd_reason="not initialized";
static void dd_close(void)
{
    if(dd_frame)IDirectDrawSurface_Release(dd_frame);
    if(dd_primary)IDirectDrawSurface_Release(dd_primary);
    if(dd_clipper)IDirectDrawClipper_Release(dd_clipper);
    if(dd_device)IDirectDraw_Release(dd_device);
    dd_frame=dd_primary=NULL;dd_clipper=NULL;dd_device=NULL;
    if(dd_library)FreeLibrary(dd_library);
    dd_library=NULL;dd_announced=0;
}
static DWORD dd_component(unsigned value,DWORD mask)
{
    unsigned shift=0;
    if(!mask)return 0;
    while(!(mask&1U)) {mask>>=1;++shift;}
    return ((value*mask+127U)/255U)<<shift;
}
static void dd_upload(void *destination,LONG pitch,const uint8_t *pixels,const uint8_t *palette)
{
    DWORD colors[256];unsigned x,y,bytes=dd_format.dwRGBBitCount/8;
    for(x=0;x<256;++x)colors[x]=dd_component(palette[x*3],dd_format.dwRBitMask)
        |dd_component(palette[x*3+1],dd_format.dwGBitMask)|dd_component(palette[x*3+2],dd_format.dwBBitMask);
    for(y=0;y<200;++y) {
        uint8_t *row=(uint8_t *)destination+(ptrdiff_t)y*pitch;
        for(x=0;x<320;++x) {
            DWORD color=colors[pixels[y*320+x]];
            if(bytes==4)((DWORD *)row)[x]=color;
            else if(bytes==2)((uint16_t *)row)[x]=(uint16_t)color;
            else {row[x*3]=(uint8_t)color;row[x*3+1]=(uint8_t)(color>>8);row[x*3+2]=(uint8_t)(color>>16);}
        }
    }
}
static int dd_open(void)
{
    typedef HRESULT (WINAPI *create_t)(GUID *,LPDIRECTDRAW *,IUnknown *);
    create_t create;FARPROC address;SDL_SysWMinfo wm;
    DDSURFACEDESC desc;DDCAPS caps;char name[32];
    const char *policy=getenv("WOLF3D_SDL1_SCALE");
    dd_reason="software requested";
    if(policy && !strcmp(policy,"software"))return 0;
    dd_reason="SDL video driver is not directx";
    if(!SDL_VideoDriverName(name,sizeof(name)) || strcmp(name,"directx"))return 0;
    dd_reason="SDL window unavailable";
    SDL_VERSION(&wm.version);
    if(!SDL_GetWMInfo(&wm))return 0;
    dd_window=wm.window;
    dd_reason="ddraw.dll unavailable";
    dd_library=LoadLibraryA("ddraw.dll");if(!dd_library)return 0;
    address=GetProcAddress(dd_library,"DirectDrawCreate");memcpy(&create,&address,sizeof(create));
    dd_reason="hardware DirectDraw device unavailable";
    if(!create || FAILED(create((GUID *)DDCREATE_HARDWAREONLY,&dd_device,NULL)))goto fail;
    dd_reason="hardware stretch capability unavailable";
    memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
    if(FAILED(IDirectDraw_GetCaps(dd_device,&caps,NULL)) || !(caps.dwCaps&DDCAPS_BLTSTRETCH))goto fail;
    dd_reason="cooperative level unavailable";
    if(FAILED(IDirectDraw_SetCooperativeLevel(dd_device,dd_window,DDSCL_NORMAL)))goto fail;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);desc.dwFlags=DDSD_CAPS;desc.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    dd_reason="primary surface unavailable";
    if(FAILED(IDirectDraw_CreateSurface(dd_device,&desc,&dd_primary,NULL)))goto fail;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(FAILED(IDirectDrawSurface_GetSurfaceDesc(dd_primary,&desc)))goto fail;
    dd_reason="unsupported display pixel format";
    dd_format=desc.ddpfPixelFormat;
    if(!(dd_format.dwFlags&DDPF_RGB) || (dd_format.dwRGBBitCount!=16 && dd_format.dwRGBBitCount!=24 && dd_format.dwRGBBitCount!=32))goto fail;
    dd_reason="window clipper unavailable";
    if(FAILED(IDirectDraw_CreateClipper(dd_device,0,&dd_clipper,NULL)) ||
       FAILED(IDirectDrawClipper_SetHWnd(dd_clipper,0,dd_window)) ||
       FAILED(IDirectDrawSurface_SetClipper(dd_primary,dd_clipper)))goto fail;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
    desc.dwWidth=320;desc.dwHeight=200;desc.ddpfPixelFormat=dd_format;
    desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
    dd_reason="video-memory upload surface unavailable";
    if(FAILED(IDirectDraw_CreateSurface(dd_device,&desc,&dd_frame,NULL)))goto fail;
    return 1;
fail:
    dd_close();return 0;
}
/* Return zero to render this same frame through the software fallback. */
static int dd_present(const uint8_t *pixels,const uint8_t *palette,int left,int top,int width,int height)
{
    DDSURFACEDESC lock;RECT dst,client,source={0,0,320,200};POINT origin={0,0};DDBLTFX fill;
    if(dd_disabled)return 0;
    if(!dd_device && !dd_open())goto fail;
    dd_reason="surface lock failed (possibly lost surface)";
    memset(&lock,0,sizeof(lock));lock.dwSize=sizeof(lock);
    if(FAILED(IDirectDrawSurface_Lock(dd_frame,NULL,&lock,DDLOCK_WAIT|DDLOCK_WRITEONLY,NULL)))goto fail;
    dd_upload(lock.lpSurface,lock.lPitch,pixels,palette);
    dd_reason="surface unlock failed";
    if(FAILED(IDirectDrawSurface_Unlock(dd_frame,lock.lpSurface)))goto fail;
    dd_reason="window geometry unavailable";
    if(!ClientToScreen(dd_window,&origin) || !GetClientRect(dd_window,&client))goto fail;
    OffsetRect(&client,origin.x,origin.y);
    dst.left=origin.x+left;dst.top=origin.y+top;dst.right=dst.left+width;dst.bottom=dst.top+height;
    /* Clip to this window so borders and stretched pixels cannot cover other windows. */
    dd_reason="border blit failed";
    if(left || top) {
        memset(&fill,0,sizeof(fill));fill.dwSize=sizeof(fill);
        if(FAILED(IDirectDrawSurface_Blt(dd_primary,&client,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fill)))goto fail;
    }
    dd_reason="hardware stretch blit failed";
    if(FAILED(IDirectDrawSurface_Blt(dd_primary,&dst,dd_frame,&source,DDBLT_WAIT,NULL)))goto fail;
    if(!dd_announced) {fprintf(stdout,"SDL1 scaling: DirectDraw hardware stretch (320x200 upload)\n");dd_announced=1;}
    return 1;
fail:
    dd_close();dd_disabled=1;
    fprintf(stdout,"SDL1 scaling: software (%s)\n",dd_reason);
    return 0;
}
#endif
