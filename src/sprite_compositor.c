#include "sprite_compositor.h"
#include "sprite_atlas.h"
static uint16_t word(const uint8_t *p) {return (uint16_t)((p[0]<<8)|p[1]);}
static uint32_t longword(const uint8_t *p) {return ((uint32_t)word(p)<<16)|word(p+2);}
int zy_sprite_commands_valid(const uint8_t *commands,unsigned count,
                            zy_sprite_read read,void *context) {
    if(count>ZY_COMPOSE_MAX_COMMANDS)return 0;
    for(unsigned i=0;i<count;i++) {
        const uint8_t *c=commands+i*ZY_COMPOSE_COMMAND_BYTES;
        uint32_t src=longword(c);unsigned dst=word(c+4),rows=c[6],cells=c[7]&127;
        if(!rows || rows>40 || cells<1 || cells>2 || (dst&3) ||
           dst+(rows-1)*80+cells*4>ZY_COMPOSE_WORDS || (src&1))return 0;
        int valid=1;
        /* Every page touched must exist, including the final mask/planes. */
        for(unsigned at=0;at<rows*10;at+=2) {
            (void)read(context,src+at,&valid);
            if(!valid)return 0;
        }
    }
    return 1;
}
int zy_sprite_compose(uint16_t *screen,const uint8_t *commands,unsigned count,
                      uint16_t hits[6],zy_sprite_read read,void *context) {
    if(!zy_sprite_commands_valid(commands,count,read,context))return 0;
    for(unsigned i=0;i<6;i++)hits[i]=0;
    for(unsigned i=0;i<count;i++) {
        const uint8_t *c=commands+i*ZY_COMPOSE_COMMAND_BYTES;
        uint32_t src=longword(c),keep=longword(c+8);
        unsigned dst=word(c+4),cells=c[7]&127,rows=c[6];int valid=1;
        for(unsigned row=0;row<rows;row++,src+=10,dst+=80) {
            uint16_t mask=read(context,src,&valid),planes[4];
            for(unsigned p=0;p<4;p++)planes[p]=read(context,src+2+p*2,&valid);
            for(unsigned cell=0;cell<cells;cell++) {
                uint32_t k=cell ? ~keep : keep;
                uint16_t *d=screen+dst+cell*4;
                for(unsigned p=0;p<4;p++) {
                    uint16_t half=(uint16_t)((p&1) ? k : k>>16);
                    if((c[7]&128) && p>=2 && (d[p]&~mask&half))hits[i/16]|=(uint16_t)(1u<<(i%16));
                    d[p]=(uint16_t)((d[p]&(mask|~half))|(planes[p]&half));
                }
            }
        }
    }
    return 1;
}
static int atlas_source_valid(uint32_t address,unsigned bytes) {
    static const uint32_t ranges[][2]={{0x19e00u,0x19f00u},{0x1a800u,0x34f2eu},{0x54f00u,0x6ec00u}};
    uint32_t end=address+bytes;
    if(end<address)return 0;
    for(unsigned i=0;i<sizeof(ranges)/sizeof(ranges[0]);i++)
        if(address>=ranges[i][0] && end<=ranges[i][1])return 1;
    return 0;
}
int zy_atlas_commands_valid(const uint8_t *commands,unsigned count) {
    if(count>ZY_COMPOSE_MAX_COMMANDS)return 0;
    for(unsigned i=0;i<count;i++) {
        const uint8_t *c=commands+i*ZY_COMPOSE_COMMAND_BYTES;
        uint32_t src=longword(c);unsigned dst=word(c+4),rows=c[6],cells=c[7]&127;
        if(!rows || rows>40 || cells<1 || cells>2 || (dst&3) ||
           dst+(rows-1)*80+cells*4>ZY_COMPOSE_WORDS || (src&1) ||
           !atlas_source_valid(src,rows*10u))return 0;
    }
    return 1;
}
int zy_atlas_compose(uint16_t *screen,const uint8_t *commands,unsigned count,
                    unsigned scene,uint16_t hits[6]) {
    if(scene>=ZY_ATLAS_SCENES)return 0;
    /* Validate the logical address range once, then read each row directly.
     * The generic path validates by fetching every sprite word before fetching
     * it again while drawing. On the RP2040 that duplicate XIP lookup is costly
     * when several sprites are composed in one frame. */
    if(!zy_atlas_commands_valid(commands,count))return 0;
    for(unsigned i=0;i<6;i++)hits[i]=0;
    for(unsigned i=0;i<count;i++) {
        const uint8_t *c=commands+i*ZY_COMPOSE_COMMAND_BYTES;
        uint32_t src=longword(c),keep=longword(c+8);
        unsigned dst=word(c+4),cells=c[7]&127,rows=c[6];
        for(unsigned row=0;row<rows;row++,src+=10,dst+=80) {
            const uint8_t *first=zy_atlas_page(scene,src);
            const uint8_t *second=0;
            unsigned offset=src&255u;
            if(offset>246u)second=zy_atlas_page(scene,(src&~255u)+256u);
            uint16_t sprite[5];
            for(unsigned p=0;p<5;p++) {
                unsigned at=offset+p*2;
                const uint8_t *page=at<256u?first:second;
                sprite[p]=word(page+(at&255u));
            }
            uint16_t mask=sprite[0];
            for(unsigned cell=0;cell<cells;cell++) {
                uint32_t k=cell ? ~keep : keep;
                uint16_t *d=screen+dst+cell*4;
                for(unsigned p=0;p<4;p++) {
                    uint16_t half=(uint16_t)((p&1)?k:k>>16);
                    if((c[7]&128) && p>=2 && (d[p]&~mask&half))
                        hits[i/16]|=(uint16_t)(1u<<(i%16));
                    d[p]=(uint16_t)((d[p]&(mask|~half))|(sprite[p+1]&half));
                }
            }
        }
    }
    return 1;
}
