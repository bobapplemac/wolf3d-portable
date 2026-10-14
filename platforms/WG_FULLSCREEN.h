#ifndef WG_FULLSCREEN_H
#define WG_FULLSCREEN_H
#include <string.h>
/* mode: -1 = default (a resolution implies display), 0 = borderless, 1 = display. */
typedef struct wg_fullscreen_options { int mode; unsigned width,height; } wg_fullscreen_options_t;
static int WG_FullscreenOption(wg_fullscreen_options_t *options,const char *name,const char *value)
{
    unsigned w=0,h=0;const char *p=value;
    if(strcmp(name,"--fullscreen-mode") && strcmp(name,"--fullscreen-resolution"))return 0;
    if(!value)return -1;
    if(!strcmp(name,"--fullscreen-mode")) {
        if(!strcmp(value,"borderless"))options->mode=0;
        else if(!strcmp(value,"display"))options->mode=1;
        else return -1;
        return 1;
    }
    if(*p<'0' || *p>'9')return -1;
    while(*p>='0' && *p<='9') {w=w*10U+(unsigned)(*p++-'0');if(w>8192)return -1;}
    if(*p!='x' && *p!='X')return -1;
    ++p;if(*p<'0' || *p>'9')return -1;
    while(*p>='0' && *p<='9') {h=h*10U+(unsigned)(*p++-'0');if(h>8192)return -1;}
    if(*p || w<320 || h<240)return -1;
    options->width=w;options->height=h;return 1;
}
static int WG_FullscreenDisplay(const wg_fullscreen_options_t *options)
{ return options->mode<0 ? options->width!=0 : options->mode; }
#define WG_FULLSCREEN_HELP "  --fullscreen-mode borderless|display  Default borderless\n  --fullscreen-resolution WxH  Display-mode resolution (default desktop)\n"
#define WG_FULLSCREEN_ERROR "--fullscreen-mode requires borderless or display; --fullscreen-resolution requires WIDTHxHEIGHT (320x240 to 8192x8192).\n"
#endif
