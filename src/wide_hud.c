/* Compact eight-scanline panel. Reads state; never commits a pickup. */
#include "machine.h"
#include "video.h"
#include "score.h"
#include "hud.h"
#include "weapon.h"
#include "player.h"
/* Written only by gameplay/front-end transitions, read by the raster ISR. */
uint8_t wide_playing;

/* Five columns, seven rows, digits then A..Z. */
static const uint8_t font[36][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static void glyph(uint8_t *panel,unsigned x,char c,unsigned inverse) {
    unsigned y,shift=26u-(x&15u);
    uint32_t mask=0x3fu<<shift;
    uint8_t *cell=panel+(x/16u)*8u;
    int index=c>='0'&&c<='9' ? c-'0' : c>='A'&&c<='Z' ? c-'A'+10 : -1;
    for(y=0;y<8;y++) {
        unsigned bits=(index>=0 && y<7) ? font[index][y]<<1 : 0;
        if(c==':' && (y==2 || y==5)) bits=8;
        if(inverse) bits^=63;
        uint32_t ink=(uint32_t)bits<<shift;
        uint8_t *word=cell+y*160u;
        uint16_t value=(uint16_t)((be16(word)&~(mask>>16))|(ink>>16));
        /* All four planes agree. Replace this six-pixel character, preserving
         * its neighbours; two long stores replace the per-pixel plane loop. */
        uint32_t planes=((uint32_t)value<<16)|value;
        wr32(word,planes);wr32(word+4,planes);
        if(mask&0xffffu) {
            value=(uint16_t)((be16(word+8)&~mask)|ink);
            planes=((uint32_t)value<<16)|value;
            wr32(word+8,planes);wr32(word+12,planes);
        }
    }
}
void wide_hud_draw(uint8_t *image,uint32_t screen) {
    char text[]="00000000 L0 V0 P0 H0  G B M S D  NEXT:SPD";
    static const char *next[5]={"SPD","PWR","BMB","SEK","MIS"};
    static uint32_t cached_panel[320];
    static uint8_t previous[11], ready;
    static char previous_text[41];
    static uint8_t previous_inverse[41];
    uint8_t state[11];
    uint8_t *panel=(uint8_t *)cached_panel;
    uint8_t *destination=image+screen+192*160;
    unsigned i,cursor=image[A_powerup_cursor],selected=image[A_selected_weapon];
    unsigned changed=!ready;
    for(i=0;i<4;i++) state[i]=image[A_player_score_bcd+i];
    state[4]=image[A_lives]; state[5]=image[A_ship_speed_level];
    state[6]=image[A_weapon_power_level]; state[7]=image[A_shield_level];
    state[8]=(uint8_t)selected; state[9]=(uint8_t)cursor;
    state[10]=image[A_trail_drone_active];
    for(i=0;i<11;i++) if(state[i]!=previous[i]) changed=1;
    if(changed) {
    for(i=0;i<11;i++) previous[i]=state[i];
    for(i=0;i<4;i++) {
        text[i*2]=(char)('0'+((image[A_player_score_bcd+i]>>4)&15));
        text[i*2+1]=(char)('0'+(image[A_player_score_bcd+i]&15));
    }
    text[10]=(char)('0'+image[A_lives]%10);
    text[13]=(char)('0'+image[A_ship_speed_level]%10);
    text[16]=(char)('0'+image[A_weapon_power_level]%10);
    text[19]=(char)('0'+image[A_shield_level]%10);
    for(i=0;i<3;i++) text[38+i]=next[cursor<5?cursor:0][i];
    if(!ready) for(i=0;i<1280;i+=4) wr32(panel+i,0);
    for(i=0;text[i];i++) {
        unsigned inverse=(i==22 && selected==3) || (i==24 && selected==1) ||
                         (i==26 && selected==2) || (i==28 && selected==4);
        /* Drone is an independent companion, not an exclusive weapon. */
        if(i==30) inverse=image[A_trail_drone_active]!=0;
        if(!ready || previous_text[i]!=text[i] || previous_inverse[i]!=inverse) {
            glyph(panel,(320u-41u*6u)/2u+i*6,text[i],inverse);
            previous_text[i]=text[i];previous_inverse[i]=(uint8_t)inverse;
        }
    }
    ready=1;
    }
    /* Real ST buffers are word aligned. Copy native longs, avoiding the
     * byte-wise generic image helpers on this per-frame path. */
#ifdef __m68k__
    for(i=0;i<320;i++) ((uint32_t *)destination)[i]=cached_panel[i];
#else
    for(i=0;i<1280;i+=4) wr32(destination+i,be32(panel+i));
#endif
}
