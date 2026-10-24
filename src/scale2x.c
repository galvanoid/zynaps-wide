#include "scale2x.h"
static uint32_t doubled(uint16_t value) {
    uint32_t x=value;
    x=(x|(x<<8))&0x00ff00ffu;x=(x|(x<<4))&0x0f0f0f0fu;
    x=(x|(x<<2))&0x33333333u;x=(x|(x<<1))&0x55555555u;
    return x|(x<<1);
}
static uint16_t get(void *screen,unsigned at,int be) {
    if(!be)return ((uint16_t *)screen)[at];
    const uint8_t *p=(const uint8_t *)screen+at*2;return (uint16_t)((p[0]<<8)|p[1]);
}
static void put(void *screen,unsigned at,int be,uint16_t value) {
    if(!be){((uint16_t *)screen)[at]=value;return;}
    uint8_t *p=(uint8_t *)screen+at*2;p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value;
}
int zy_scale2x_draw(void *screen,int be,uint32_t source,int x,int y,unsigned rows,
                    int collide,zy_sprite_read read,void *context) {
    if(!rows || rows>40 || (source&1))return -1;
    if(x<=-32 || x>=320 || y>=192 || y+(int)rows*2<=0)return 0;
    int cell=x>=0?x/16:-((15-x)/16);
    unsigned phase=(unsigned)(x-cell*16),cells=phase?3u:2u;
    int hit=0,valid=1;
    for(unsigned row=0;row<rows;row++,source+=10) {
        uint16_t words[5][3];
        for(unsigned p=0;p<5;p++) {
            uint32_t bits=doubled(read(context,source+p*2,&valid));
            words[p][0]=(uint16_t)(bits>>(16+phase));
            words[p][1]=(uint16_t)(bits>>phase);
            words[p][2]=phase?(uint16_t)(bits<<(16-phase)):0;
            if(!p && phase) {
                words[p][0]|=(uint16_t)(0xffffu<<(16-phase));
                words[p][2]|=(uint16_t)(0xffffu>>phase);
            }
        }
        if(!valid)return -1;
        for(unsigned repeat=0;repeat<2;repeat++) {
            int line=y+(int)row*2+(int)repeat;if(line<0 || line>=192)continue;
            for(unsigned c=0;c<cells;c++) {
                int column=cell+(int)c;if(column<0 || column>=20)continue;
                unsigned at=(unsigned)line*80+(unsigned)column*4;
                uint16_t mask=words[0][c];
                for(unsigned p=0;p<4;p++) {
                    uint16_t old=get(screen,at+p,be);
                    if(collide && p>=2 && (old&~mask))hit=1;
                    put(screen,at+p,be,(uint16_t)((old&mask)|words[p+1][c]));
                }
            }
        }
    }
    return hit;
}
