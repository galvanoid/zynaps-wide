#include "scale2x.h"
#include "sprite_atlas.h"
#include <string.h>
static unsigned word(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t longword(const uint8_t *p){return ((uint32_t)word(p)<<16)|word(p+2);}
static uint16_t atlas_word(void *context,uint32_t address,int *valid) {
    unsigned scene=*(const unsigned *)context;
    const uint8_t *p=zy_atlas_page(scene,address);
    if(!p){*valid=0;return 0;}
    unsigned offset=address&255u;
    return (uint16_t)((p[offset]<<8)|p[offset+1]);
}
int zy_boss_compose(uint16_t *screen,const uint8_t *commands,unsigned count,
                    unsigned scene,uint16_t hits[6]) {
    if(scene>=ZY_ATLAS_SCENES || count>ZY_COMPOSE_MAX_COMMANDS)return 0;
    /* Validate the complete packet before touching the output. */
    for(unsigned i=0;i<count;i++) {
        const uint8_t *c=commands+i*16;uint8_t ordinary[12];
        memcpy(ordinary,c,12);
        if(c[7]&ZY_COMMAND_SCALE2) {
            if((c[7]&127)!=(ZY_COMMAND_SCALE2|1u))return 0;
            ordinary[4]=ordinary[5]=0;ordinary[7]=(uint8_t)(c[7]&~ZY_COMMAND_SCALE2);
        }
        if(!zy_atlas_commands_valid(ordinary,1))return 0;
    }
    memset(hits,0,12);
    for(unsigned i=0;i<count;i++) {
        const uint8_t *c=commands+i*16;int hit=0;
        if(c[7]&ZY_COMMAND_SCALE2) {
            hit=zy_scale2x_draw(screen,0,longword(c),(int16_t)word(c+12),(int16_t)word(c+14),
                              c[6],(c[7]&128)!=0,atlas_word,&scene);
        } else {
            uint16_t one[6];
            if(!zy_atlas_compose(screen,c,1,scene,one))return 0;
            hit=(one[0]&1)!=0;
        }
        if(hit>0)hits[i/16]|=(uint16_t)(1u<<(i%16));
    }
    return 1;
}
