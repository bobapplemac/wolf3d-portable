#ifndef WG_NEAREST_SCALE_H
#define WG_NEAREST_SCALE_H
#include <stdint.h>
#include <string.h>
/* Positive dimensions, tightly packed 32-bit destination; caller clips/aspects. */
static void WG_NearestScale(uint32_t *dst, unsigned stride, unsigned width,
    unsigned height, const uint8_t *src, unsigned sw, unsigned sh,
    const uint8_t *palette, unsigned *xmap)
{
    uint32_t colors[256];
    unsigned x,y,last=~0U;
    for(x=0;x<256;++x) colors[x]=((uint32_t)palette[x*3]<<16)
        |((uint32_t)palette[x*3+1]<<8)|palette[x*3+2];
    for(x=0;x<width;++x)xmap[x]=(unsigned)((uint64_t)x*sw/width);
    for(y=0;y<height;++y) {
        unsigned sy=(unsigned)((uint64_t)y*sh/height);
        uint32_t *row=dst+(size_t)y*stride;
        if(sy==last) memcpy(row,row-stride,(size_t)width*sizeof(*row));
        else for(x=0;x<width;++x)row[x]=colors[src[(size_t)sy*sw+xmap[x]]];
        last=sy;
    }
}
#endif
