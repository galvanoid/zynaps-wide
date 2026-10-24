#include "sprite_commands.h"
#include "machine.h"
#include "entity.h"
#include "sprite.h"
#include "video.h"
/* Commands start after the five-byte header. On 68000 this is an odd
 * address: image accessors must NEVER be used to store into the wire packet.
 * Volatile byte stores also prevent GCC combining them into MOVE.W/L. */
static void wire_word(uint8_t *p,unsigned value) {
    volatile uint8_t *v=p;v[0]=(uint8_t)(value>>8);v[1]=(uint8_t)value;
}
static void wire_long(uint8_t *p,uint32_t value) {
    wire_word(p,value>>16);wire_word(p+2,value);
}
#ifndef ZY_SPRITE_COMMAND_NAME
#define ZY_SPRITE_COMMAND_NAME zy_sprite_command
#endif
int ZY_SPRITE_COMMAND_NAME(const uint8_t *image,uint32_t entity,unsigned step,
                      int collide,uint8_t c[12]) {
    int x=(int16_t)(be16(image+entity+ENTITY_X)&0xfffe),y=(int16_t)be16(image+entity+ENTITY_Y);
    unsigned height=be16(image+entity+ENTITY_HEIGHT),cells=collide?2:1,dst=0;
    uint32_t keep=0xffffffffu;
    if(collide)height&=0x7fff;
    if(!height || height>40)return -1;
    if(y>=224 || y+(int)height<=32)return 0;
    if(!collide && (x<0 || x>=320))return 0;
    /* Both factors fit in 16 bits.  Keeping that fact visible to GCC makes
     * the 68000 use its single MULU.W instead of calling __mulsi3 twice per
     * sprite.  This path runs once for every visible enemy and projectile. */
    {
        uint16_t phase=(uint16_t)(x&15);
        uint16_t stride=(uint16_t)(collide?height*5u:step);
        uint32_t src=be32(image+entity+ENTITY_SPRITE)+(uint32_t)phase*stride;
    unsigned rows=height;
    if(y<32) {unsigned hidden=(unsigned)(32-y);src+=hidden*10;rows-=hidden;}
    else {if(y+(int)rows>224)rows=(unsigned)(224-y);dst=(unsigned)(y-32)*80;}
    if(collide) {
        keep=be32(image+A_shift_mask_table+(unsigned)(x&15)*2);
        if(x<64) {if(x<=48)return 0;keep=~keep;cells=1;}
        else if(x<=368)dst+=(unsigned)((x-64)&0xfff0)/4;
        else if(x>=384)return 0;
        else {dst+=76;cells=1;}
    } else dst+=(unsigned)(x&0xfff0)/4;
    /* At phase zero the far cell has keep=0 and cannot change pixels or
     * collisions. Omit it, including x=368 on the last gameplay row. */
    if(cells==2 && keep==0xffffffffu)cells=1;
    wire_long(c,src);wire_word(c+4,dst);c[6]=(uint8_t)rows;
    c[7]=(uint8_t)(cells|(collide?128:0));wire_long(c+8,keep);
    return 1;
    }
}
