#ifndef WG_NEAREST_SCALE_H
#define WG_NEAREST_SCALE_H
#include <stdint.h>
#include <string.h>
static void WG_NearestMap(unsigned *map,unsigned width,unsigned source_width)
{
    unsigned x;
    for(x=0;x<width;++x)map[x]=(unsigned)((uint64_t)x*source_width/width);
}
/* Positive dimensions, tightly packed 32-bit destination; caller clips/aspects. */
static void WG_NearestScale(uint32_t *dst, unsigned stride, unsigned width,
    unsigned height, const uint8_t *src, unsigned sw, unsigned sh,
    const uint8_t *palette, unsigned *xmap)
{
    uint32_t colors[256];
    unsigned x,y,last=~0U;
    for(x=0;x<256;++x) colors[x]=((uint32_t)palette[x*3]<<16)
        |((uint32_t)palette[x*3+1]<<8)|palette[x*3+2];
    for(y=0;y<height;++y) {
        unsigned sy=(unsigned)((uint64_t)y*sh/height);
        uint32_t *row=dst+(size_t)y*stride;
        if(sy==last) memcpy(row,row-stride,(size_t)width*sizeof(*row));
        else for(x=0;x<width;++x)row[x]=colors[src[(size_t)sy*sw+xmap[x]]];
        last=sy;
    }
}
static void WG_NearestScale16(uint16_t *dst,unsigned stride,unsigned width,
    unsigned height,const uint8_t *src,unsigned sw,unsigned sh,
    const uint8_t *palette,const unsigned *xmap,int rgb565)
{
    uint16_t colors[256];
    unsigned x,y,last=~0U;
    for(x=0;x<256;++x) colors[x]=(uint16_t)(
        ((unsigned)(palette[x*3]>>3)<<(rgb565?11:10)) |
        ((unsigned)(palette[x*3+1]>>(rgb565?2:3))<<5) | (palette[x*3+2]>>3));
    for(y=0;y<height;++y) {
        unsigned sy=(unsigned)((uint64_t)y*sh/height);
        uint16_t *row=dst+(size_t)y*stride;
        if(sy==last)memcpy(row,row-stride,(size_t)width*sizeof(*row));
        else for(x=0;x<width;++x)row[x]=colors[src[(size_t)sy*sw+xmap[x]]];
        last=sy;
    }
}
#endif
