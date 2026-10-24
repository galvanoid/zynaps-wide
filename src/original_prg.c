#include "original_prg.h"
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
uint32_t zy_crc32(uint32_t crc,const uint8_t *p,unsigned n){
    static uint32_t table[256];static int initialized;
    if(!initialized){
        for(unsigned i=0;i<256;i++){
            uint32_t v=i;for(unsigned bit=0;bit<8;bit++)v=(v>>1)^(0xedb88320u&(0u-(v&1u)));
            table[i]=v;
        }
        initialized=1;
    }
    while(n--)crc=(crc>>8)^table[(crc^*p++)&255u];
    return crc;
}
int zy_original_header(const uint8_t h[28]){
    return h[0]==0x60 && h[1]==0x1a && be32(h+2)==ZY_ORIGINAL_TEXT &&
        !be32(h+6) && be32(h+10)==0x54a28u && !be32(h+14) && !h[26] && !h[27];
}
/* Validate the complete stream before touching text. GEMDOS relocation byte
 * 1 advances 254 bytes without fixing a pointer; 0 terminates the stream. */
static int walk(uint8_t *text,const uint8_t *r,unsigned n,int apply){
    if(n<4)return 0;
    uint32_t offset=be32(r);unsigned cursor=4;
    if(!offset)return n==4;
    for(;;){
        if((offset&1u) || offset>ZY_ORIGINAL_TEXT-4)return 0;
        if(apply){uint32_t v=be32(text+offset)+0x10000u;
            text[offset]=(uint8_t)(v>>24);text[offset+1]=(uint8_t)(v>>16);
            text[offset+2]=(uint8_t)(v>>8);text[offset+3]=(uint8_t)v;}
        unsigned delta;
        do{
            if(cursor>=n)return 0;
            delta=r[cursor++];
            if(!delta)return cursor==n;
            if(delta==1){if(offset>ZY_ORIGINAL_TEXT-254u)return 0;offset+=254;}
        }while(delta==1);
        if(offset>ZY_ORIGINAL_TEXT-delta)return 0;
        offset+=delta;
    }
}
int zy_original_relocate(uint8_t *text,const uint8_t *relocations,unsigned length){
    return walk(text,relocations,length,0) && walk(text,relocations,length,1);
}
