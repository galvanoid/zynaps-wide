#include "tile_decode.h"
static unsigned word(const uint8_t *p) { return ((unsigned)p[0]<<8)|p[1]; }
int zy_decode_tiles(const uint8_t *bank,unsigned bytes,const uint8_t *map,uint8_t *out) {
    unsigned t,y,b,h;
    /* Validate all indices before touching the output. */
    for(t=0;t<36;t++) if((word(map+t*2)&0x7fff)>=bytes/64) return -1;
    for(t=0;t<18;t++) for(y=0;y<8;y++) for(b=0;b<8;b+=2) for(h=0;h<2;h++) {
        unsigned id=word(map+t*2+h*36);
        unsigned row=(id&0x8000) ? 7-y : y;
        unsigned at=(id&0x7fff)*64+row*8+b;
        *out++=bank[at]; *out++=bank[at+1];
    }
    return 0;
}
