#include "boss2x.h"
#include "scale2x.h"
#include "machine.h"
#include "entity.h"
#include "player.h"
#include "enemy.h"
#include "mothership.h"
#include "collision.h"
#include "init.h"
#include "video.h"
#include "frame.h"
#include "sprite.h"
int zy_boss2x_enabled;
static int geometry(const uint8_t *image,uint32_t entity,int *x,int *y,unsigned *rows,uint32_t *source) {
    if(!zy_boss2x_enabled || (!image[A_boss_sequence_active] && !image[A_mothership_ready] && !image[A_mothership_prep_stage]))return 0;
    int ex=(int16_t)be16(image+entity+ENTITY_X),ey=(int16_t)be16(image+entity+ENTITY_Y);
    *rows=be16(image+entity+ENTITY_HEIGHT)&0x7fff;
    *source=be32(image+entity+ENTITY_SPRITE);
    if(!*rows || *rows>40)return 0;
    if(image[A_mothership_index]<5) {
        int tail=entity>=A_entity_boss_parts && entity<A_entity_boss_parts+5u*ENTITY_STRIDE;
        int head=entity>=A_enemy_slots && entity<A_enemy_slots+2u*ENTITY_STRIDE && image[entity+ENTITY_TYPE]==MOTHERSHIP_HEAD_TYPE;
        if(!tail && !head)return 0;
        int sx=ex-(head?64:0),sy=ey-32;
        *x=2*sx-(int16_t)be16(image+A_mothership_x)-32;
        *y=2*sy-((int16_t)be16(image+A_mothership_y)-32)-20;
    } else {
        if(entity<A_enemy_slots || entity>=A_enemy_slots+8u*ENTITY_STRIDE || image[entity+ENTITY_TYPE]!=MOTHERSHIP_SEGMENT_TYPE)return 0;
        uint32_t parent=A_enemy_slots;
        while(entity-parent>=2u*ENTITY_STRIDE)parent+=2u*ENTITY_STRIDE;
        *x=2*(ex-64)-((int16_t)be16(image+parent+ENTITY_X)-64)-16;
        *y=2*(ey-32)-((int16_t)be16(image+parent+ENTITY_Y)-32)-8;
    }
    return 1;
}
static void wire16(uint8_t *p,unsigned v){volatile uint8_t *q=p;q[0]=(uint8_t)(v>>8);q[1]=(uint8_t)v;}
int zy_boss2x_command(const uint8_t *image,uint32_t entity,int collide,uint8_t *out,int16_t *dx,int16_t *dy) {
    int x,y;unsigned rows;uint32_t source;
    if(!geometry(image,entity,&x,&y,&rows,&source))return 0;
    wire16(out,source>>16);wire16(out+2,source);wire16(out+4,0);
    volatile uint8_t *wire=out;
    wire[6]=(uint8_t)rows;wire[7]=(uint8_t)(ZY_COMMAND_SCALE2|1u|(collide?128u:0));
    for(unsigned i=8;i<12;i++)wire[i]=255;
    *dx=(int16_t)x;*dy=(int16_t)y;return 1;
}
static uint16_t native_word(void *context,uint32_t address,int *valid) {
    if(address+1>=0x80000u){*valid=0;return 0;}
    return be16((uint8_t *)context+address);
}
int zy_boss2x_native(uint8_t *image,uint32_t entity,uint32_t hit,int collide) {
    int x,y;unsigned rows;uint32_t source;
    if(!geometry(image,entity,&x,&y,&rows,&source))return 0;
    int result=zy_scale2x_draw(image+be32(image+A_screen_back),1,source,x,y,rows,collide,native_word,image);
    if(result>0 && collide)image[hit]=255;
    return result>=0;
}
static void box(const uint8_t *image,uint32_t entity,int *x,int *y,int *right,int *bottom) {
    unsigned rows;uint32_t source;
    if(geometry(image,entity,x,y,&rows,&source)) {
        *x+=64;*y+=32;*right=*x+32;*bottom=*y+(int)rows*2;
    } else {
        *x=(int16_t)be16(image+entity+ENTITY_X);*y=(int16_t)be16(image+entity+ENTITY_Y);
        *right=(int16_t)(uint16_t)(*x+16);
        *bottom=(int16_t)(uint16_t)(*y+(be16(image+entity+ENTITY_HEIGHT)&0x7fff));
    }
}
void zy_boss2x_pairs(uint8_t *image) {
    if(!zy_boss2x_enabled || (!image[A_boss_sequence_active] && !image[A_mothership_ready]))return;
    /* Preserve the original ordered walk, pixel-hit filter and reciprocal masks.
     * Actor positions/height and scripts are never modified for this box pass. */
    for(unsigned i=0;i<COLLISION_MASK_LONGS;i++)wr32(image+A_entity_collision_masks+i*4,0);
    for(unsigned left=1;left<ENTITY_SLOTS;left++) {
        uint32_t l=A_entity_table+left*ENTITY_STRIDE;
        if(!image[l+ENTITY_PIXEL_HIT])continue;
        int lx,ly,lr,lb;box(image,l,&lx,&ly,&lr,&lb);
        for(unsigned right=0;right<left;right++) {
            uint32_t r=A_entity_table+right*ENTITY_STRIDE;
            if(!image[r+ENTITY_ALIVE])continue;
            int rx,ry,rr,rb;box(image,r,&rx,&ry,&rr,&rb);
            if(ry>=lb || lx>=rr || rx>=lr || ly>=rb)continue;
            wr32(image+A_entity_collision_masks+left*4,be32(image+A_entity_collision_masks+left*4)|(1u<<right));
            wr32(image+A_entity_collision_masks+right*4,be32(image+A_entity_collision_masks+right*4)|(1u<<left));
        }
    }
}
