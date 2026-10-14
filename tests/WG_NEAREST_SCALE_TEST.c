#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../platforms/win32/WG_NEAREST_SCALE.h"
int main(void)
{
    uint8_t src[6]={1,2,3,4,5,6},palette[768]={0};
    uint32_t dst[80]; unsigned map[10],x,y;
    for(x=0;x<6;++x)palette[(x+1)*3]=(uint8_t)(x+1);
    for(x=0;x<80;++x)dst[x]=0xdeadbeef;
    WG_NearestScale(dst+1,10,7,5,src,3,2,palette,map);
    for(y=0;y<5;++y)for(x=0;x<7;++x)
        assert(dst[1+y*10+x]==(uint32_t)src[(y*2/5)*3+x*3/7]<<16);
    for(y=0;y<5;++y)assert(dst[y*10]==0xdeadbeef && dst[y*10+8]==0xdeadbeef);
    WG_NearestScale(dst,10,2,1,src,3,2,palette,map);
    assert(dst[0]==0x10000 && dst[1]==0x20000);
    puts("Nearest scaling: palette, repeated rows, odd ratios, downscale and stride guards passed");
    return 0;
}
