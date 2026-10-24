/* No GEMDOS or debug traffic during play. Includes the frame's raster/VBL wait.
 * Clock granularity: TOS 200 Hz (5 ms) and the actual VBL counter (20 ms PAL).
 * Buckets: scenery/asteroids, each with zero or at least one live player shot. */
#include "frame_meter.h"
#include "machine.h"
#include "entity.h"
#include "player.h"
#include "frame.h"
#include "init.h"
#include "mothership.h"
#include "sprite.h"
static uint32_t report[4+ZY_METER_BUCKETS*ZY_METER_FIELDS]={0x5a594d31u,ZY_METER_VERSION,ZY_METER_BUCKETS,ZY_METER_FIELDS};
static uint32_t start_ticks,start_vbl;
static unsigned armed,bucket,shots,enemies;
#ifdef ZY_FIX_TIMING
static const uint8_t *frame_image;
static uint32_t stage_start[4],stage_elapsed[4],wait_start,wait_elapsed,before_wait;
static uint8_t stage_live[4],wait_live;
static uint32_t component_start[4],component_elapsed[4];
static uint8_t component_live[4];
#endif
#ifdef ZY_METER_TEST
extern uint32_t zy_meter_test_ticks,zy_meter_test_vbl;
static uint32_t ticks(void){return zy_meter_test_ticks;}
static uint32_t vbl(void){return zy_meter_test_vbl;}
#else
extern volatile uint32_t zy_vbl_ticks;
static uint32_t ticks(void){return *(volatile uint32_t *)(uintptr_t)0x4ba;}
static uint32_t vbl(void){return zy_vbl_ticks;}
#endif
void zy_meter_begin(const uint8_t *image) {
    start_ticks=ticks();start_vbl=vbl();shots=enemies=0;
    for(unsigned i=0;i<ENTITY_SLOTS;i++)if(image[A_entity_table+i*ENTITY_STRIDE+ENTITY_ALIVE]) {
        if(i<6)shots++;else enemies++;
    }
    bucket=(image[A_asteroid_section_flag]?2u:0u)+(shots?1u:0u);armed=1;
#ifdef ZY_FIX_TIMING
    frame_image=image;wait_live=0;wait_elapsed=before_wait=0;
    for(unsigned i=0;i<4;i++){stage_live[i]=component_live[i]=0;stage_elapsed[i]=component_elapsed[i]=0;}
#endif
}
void zy_meter_stage_begin(unsigned stage) {
#ifdef ZY_FIX_TIMING
    if(armed && stage<4){stage_start[stage]=ticks();stage_live[stage]=1;}
#else
    (void)stage;
#endif
}
void zy_meter_stage_end(unsigned stage) {
#ifdef ZY_FIX_TIMING
    if(armed && stage<4 && stage_live[stage]){stage_elapsed[stage]+=ticks()-stage_start[stage];stage_live[stage]=0;}
#else
    (void)stage;
#endif
}
void zy_meter_wait_begin(void) {
#ifdef ZY_FIX_TIMING
    if(armed && !wait_live){wait_start=ticks();before_wait=wait_start-start_ticks;wait_live=1;}
#endif
}
void zy_meter_wait_end(void) {
#ifdef ZY_FIX_TIMING
    if(armed && wait_live){wait_elapsed+=ticks()-wait_start;wait_live=0;}
#endif
}
uint32_t zy_meter_clock(void){return ticks();}
void zy_meter_component_add(unsigned component,uint32_t elapsed) {
#ifdef ZY_FIX_TIMING
    if(armed && component<4)component_elapsed[component]+=elapsed;
#else
    (void)component;(void)elapsed;
#endif
}
void zy_meter_component_begin(unsigned component) {
#ifdef ZY_FIX_TIMING
    if(armed && component<4){component_start[component]=ticks();component_live[component]=1;}
#else
    (void)component;
#endif
}
void zy_meter_component_end(unsigned component) {
#ifdef ZY_FIX_TIMING
    if(armed && component<4 && component_live[component]) {
        component_elapsed[component]+=ticks()-component_start[component];component_live[component]=0;
    }
#else
    (void)component;
#endif
}
void zy_meter_end(void) {
    uint32_t elapsed_ticks=ticks()-start_ticks,elapsed_vbl=vbl()-start_vbl;
    if(!armed)return;
#ifdef ZY_FIX_TIMING
    zy_meter_wait_end();
    if(frame_image[A_boss_sequence_active] || frame_image[A_mothership_ready] || frame_image[A_mothership_pending])bucket=4u+(shots?1u:0u);
#endif
    uint32_t *b=report+4+bucket*ZY_METER_FIELDS;
    b[0]++;b[1]+=elapsed_ticks;if(elapsed_ticks>b[2])b[2]=elapsed_ticks;
    b[3]+=elapsed_vbl;if(elapsed_vbl>b[4])b[4]=elapsed_vbl;
    b[5]+=shots;b[6]+=enemies;if(enemies>b[7])b[7]=enemies;
    b[8+(elapsed_vbl<15?elapsed_vbl:15)]++;
#ifdef ZY_FIX_TIMING
    for(unsigned i=0;i<4;i++) {
        b[24+i*2]+=stage_elapsed[i];
        if(stage_elapsed[i]>b[25+i*2])b[25+i*2]=stage_elapsed[i];
    }
    b[32]+=before_wait;if(before_wait>b[33])b[33]=before_wait;
    b[34]+=wait_elapsed;if(wait_elapsed>b[35])b[35]=wait_elapsed;
    uint32_t work=elapsed_ticks>=wait_elapsed?elapsed_ticks-wait_elapsed:0;
    b[36]+=work;if(work>b[37])b[37]=work;
    b[38]+=work>4;b[39]+=work>8;
    for(unsigned i=0;i<4;i++) {
        b[40+i*2]+=component_elapsed[i];
        if(component_elapsed[i]>b[41+i*2])b[41+i*2]=component_elapsed[i];
    }
#endif
    armed=0;
}
const uint32_t *zy_meter_report(unsigned *bytes){*bytes=sizeof report;return report;}
