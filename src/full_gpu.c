#ifdef ZY_FULL_GPU
#include "full_gpu.h"
#include "next_features.h"
#include "terrain.h"
#include "sprite_commands.h"
#include "sprite_compositor.h"
#include "protocol.h"
#include "machine.h"
#include "entity.h"
#include "sprite.h"
#include "mothership.h"
#include "init.h"
#include "scroll.h"
#include "video.h"
#include <string.h>
extern int zy_sc_rpc(unsigned,const uint8_t *,unsigned);
extern int zy_sc_is_active(void);
extern void zy_sc_gpu_failed(uint8_t *);
extern void zy_sc_clear_playfield(void *);
extern void zy_sc_workspace_edge(const void *,void *);
extern void zy_sc_copy_scene(const volatile void *,void *);
extern void zy_sc_copy_scene_blitter(const volatile void *,void *);
extern void zy_sc_copy_scene_blitter_start(const volatile void *,void *);
extern void zy_sc_copy_scene_blitter_wait(void);
extern int zy_sc_blitter_busy(void);
extern void zy_sc_copy_rects_blitter(const volatile void *,void *,const uint8_t *,unsigned);
extern int zy_sc_has_blitter(void);
extern void __real_draw_sprite_masked_asm(uint8_t *,uint32_t,uint16_t);
extern void __real_draw_sprite_masked_collide_asm(uint8_t *,uint32_t,uint32_t);
#define PHASE(n) extern void __real_scroll_page_to_screen_p##n##_asm(uint8_t *,uint32_t,uint32_t);
PHASE(00) PHASE(01) PHASE(02) PHASE(03) PHASE(04) PHASE(05) PHASE(06) PHASE(07) PHASE(08) PHASE(09)
PHASE(10) PHASE(11) PHASE(12) PHASE(13) PHASE(14) PHASE(15) PHASE(16) PHASE(17) PHASE(18) PHASE(19)
#define ENTRY(n) __real_scroll_page_to_screen_p##n##_asm
static void (*const native_phase[20])(uint8_t *,uint32_t,uint32_t)={
ENTRY(00),ENTRY(01),ENTRY(02),ENTRY(03),ENTRY(04),ENTRY(05),ENTRY(06),ENTRY(07),ENTRY(08),ENTRY(09),
ENTRY(10),ENTRY(11),ENTRY(12),ENTRY(13),ENTRY(14),ENTRY(15),ENTRY(16),ENTRY(17),ENTRY(18),ENTRY(19)};
static uint8_t *frame_image;
static uint8_t payload[5+12*ZY_COMPOSE_MAX_COMMANDS];
static struct {uint32_t entity,hit;uint16_t step;uint8_t fields[14],collide;} records[ZY_COMPOSE_MAX_COMMANDS];
static unsigned count,collecting,blocked,clear_background,background_page,background_phase;
static unsigned blitter_checked,blitter_available,dirty_mode,copy_pending;
#ifdef ZY_PROFILE
static uint32_t copy_started;
uint32_t zy_gpu_overlap_ticks,zy_gpu_wait_ticks,zy_gpu_busy_frames;
#endif
static uint32_t raw_page,screen;
volatile uint32_t zy_gpu_frames,zy_gpu_sprites,zy_gpu_fallbacks;
#ifdef ZY_PROFILE
uint32_t zy_gpu_queue_ticks,zy_gpu_rpc_ticks,zy_gpu_copy_ticks;
static uint32_t gpu_clock(void) {
    uint32_t value;__asm__ volatile("move.l 0x4ba,%0":"=d"(value)::"memory");return value;
}
#define GPU_TIME(name) uint32_t name=gpu_clock()
#define GPU_ADD(name,field) field+=gpu_clock()-name
#else
#define GPU_TIME(name) ((void)0)
#define GPU_ADD(name,field) ((void)0)
#endif
static unsigned cart(unsigned offset) {
#ifdef ZY_SC_TEST
    extern uint16_t *zy_test_cart(unsigned);
    return *zy_test_cart(offset);
#else
    return *(volatile uint16_t *)(uintptr_t)(0xfa0000UL+offset);
#endif
}
static int blitter_supported(void) {
    if(!blitter_checked) {blitter_available=zy_sc_has_blitter()!=0;blitter_checked=1;}
    return blitter_available;
}
#ifdef ZY_DIRTY_EXPERIMENTAL
static int dirty_supported(void) {
    return blitter_supported() && cart(ZY_DIRTY_CAPS_OFFSET)==ZY_DIRTY_CAPS_MAGIC &&
        cart(ZY_DIRTY_CAPS_OFFSET+2)==(uint16_t)~ZY_DIRTY_CAPS_MAGIC;
}
#endif
void zy_gpu_present(void) {
    if(!copy_pending)return;
#ifdef ZY_PROFILE
    {
        uint32_t wait_started=gpu_clock();
        zy_gpu_overlap_ticks+=wait_started-copy_started;
        zy_gpu_busy_frames+=(unsigned)zy_sc_blitter_busy();
        zy_sc_copy_scene_blitter_wait();
        zy_gpu_wait_ticks+=gpu_clock()-wait_started;
        zy_gpu_copy_ticks+=gpu_clock()-copy_started;
    }
#else
    zy_sc_copy_scene_blitter_wait();
#endif
    copy_pending=0;
}
void zy_gpu_reset(void) {zy_gpu_present();blocked=0;collecting=0;}
void zy_gpu_abort(void) {zy_gpu_present();if(collecting)zy_gpu_fallbacks++;collecting=0;blocked=1;}
int zy_gpu_collecting(void) {return collecting!=0;}
void zy_gpu_begin(uint8_t *image) {
    frame_image=image;count=0;clear_background=1;
    screen=be32(image+A_screen_back);
    /* Gameplay is now fully initialized, so probing XBIOS Blitmode is safe.
     * The stable full-frame path also needs this result to select the reverse
     * Sidecart layout consumed directly by the STE Blitter. */
    (void)blitter_supported();
#ifdef ZY_DIRTY_EXPERIMENTAL
    dirty_mode=dirty_supported();
#else
    dirty_mode=0;
#endif
#ifdef ZY_GPU_DISABLE
    collecting=0;return;
#endif
    collecting=!blocked && cart(ZY_FULL_CAPS_OFFSET)==ZY_FULL_CAPS_MAGIC &&
        cart(ZY_FULL_CAPS_OFFSET+2)==(uint16_t)~ZY_FULL_CAPS_MAGIC &&
        (zy_sc_is_active() || image[A_boss_sequence_active] || image[A_asteroid_section_flag]);
}
int zy_gpu_clear(uint8_t *image) {
    if(!collecting || image!=frame_image)return 0;
    clear_background=1;return dirty_mode ? 0 : 1;
}
int zy_gpu_defer(uint8_t *image,uint32_t page,uint32_t destination,unsigned phase) {
    if(!collecting || image!=frame_image || phase>=20)return 0;
    for(unsigned p=0;p<8;p++) if(be32(image+A_map_page_table+p*4)==page) {
        clear_background=0;background_page=p;background_phase=phase;raw_page=page;screen=destination;
        return dirty_mode ? 2 : 1;
    }
    return 0;
}
int zy_gpu_native_background(void) {
    /* Prefill runs before the first gameplay frame, in a context where an
     * XBIOS Blitmode trap is unsafe.  Maintaining the native pages only needs
     * the firmware capability; actual dirty copying still checks the Blitter
     * later from zy_gpu_begin(), where 0.4.2 already proved the call safe. */
#ifdef ZY_DIRTY_EXPERIMENTAL
    return cart(ZY_DIRTY_CAPS_OFFSET)==ZY_DIRTY_CAPS_MAGIC &&
        cart(ZY_DIRTY_CAPS_OFFSET+2)==(uint16_t)~ZY_DIRTY_CAPS_MAGIC;
#else
    return 0;
#endif
}
static void fallback(void) {
    collecting=0;blocked=1;zy_gpu_fallbacks++;
    zy_sc_gpu_failed(frame_image);
    if(clear_background)zy_sc_clear_playfield(frame_image+screen);
    else {
        extern void zy_sc_clear_gap(void *);
        if(!zy_sc_draw_native_layout(frame_image,raw_page,screen,background_phase)) {
            zy_sc_clear_gap(frame_image+screen);
            native_phase[background_phase](frame_image,raw_page,screen);
            zy_sc_workspace_edge(frame_image+A_scroll_col_workspace,frame_image+screen+152);
        }
    }
    for(unsigned i=0;i<count;i++) {
        uint8_t saved[14];memcpy(saved,frame_image+records[i].entity,14);
        memcpy(frame_image+records[i].entity,records[i].fields,14);
        if(records[i].collide)__real_draw_sprite_masked_collide_asm(frame_image,records[i].entity,records[i].hit);
        else __real_draw_sprite_masked_asm(frame_image,records[i].entity,records[i].step);
        memcpy(frame_image+records[i].entity,saved,14);
    }
    count=0;
}
static int enqueue(uint8_t *image,uint32_t entity,unsigned step,uint32_t hit,int collide) {
    if(!collecting || image!=frame_image)return 0;
    if(count==ZY_COMPOSE_MAX_COMMANDS) {fallback();return 0;}
    GPU_TIME(queue_start);
    int emitted=zy_sprite_command(image,entity,step,collide,payload+5+count*12);
    if(emitted<0) {fallback();return 0;}
    if(!emitted)return 1;
    records[count].entity=entity;records[count].hit=hit;records[count].step=(uint16_t)step;
    records[count].collide=(uint8_t)collide;memcpy(records[count].fields,image+entity,14);
    GPU_ADD(queue_start,zy_gpu_queue_ticks);
    count++;return 1;
}
void __wrap_draw_sprite_masked_asm(uint8_t *image,uint32_t entity,uint16_t step) {
    if(!enqueue(image,entity,step,0,0))__real_draw_sprite_masked_asm(image,entity,step);
}
void __wrap_draw_sprite_masked_collide_asm(uint8_t *image,uint32_t entity,uint32_t hit) {
    if(!enqueue(image,entity,0,hit,1))__real_draw_sprite_masked_collide_asm(image,entity,hit);
}
void zy_gpu_flush(void) {
    if(!collecting)return;
    unsigned mode=0;
    if(frame_image[A_boss_sequence_active])mode=4;
#ifdef ZY_FIX_BOSS
    /* Non-segmented bosses already use the boss bank while flying in, before
     * ready/active is set. Scene zero contains no boss pixels or hit mask. */
    else if(frame_image[A_mothership_index]<5 && frame_image[A_mothership_prep_stage])mode=4;
#endif
    else if(frame_image[A_mothership_index]>=5) {
        if(frame_image[A_mothership_prep_stage]==2)mode=1;
        else if(frame_image[A_mothership_prep_stage]==3)mode=2;
        else if(frame_image[A_mothership_ready])mode=3;
    }
    payload[0]=(uint8_t)(frame_image[A_level_section]*5+mode);
    payload[1]=(uint8_t)background_page;payload[2]=(uint8_t)background_phase;
    payload[3]=(uint8_t)clear_background;payload[4]=(uint8_t)count;
    GPU_TIME(rpc_start);
    int reverse=!dirty_mode && blitter_available && cart(ZY_REVERSE_CAPS_OFFSET)==ZY_REVERSE_CAPS_MAGIC &&
        cart(ZY_REVERSE_CAPS_OFFSET+2)==(uint16_t)~ZY_REVERSE_CAPS_MAGIC;
    unsigned opcode=dirty_mode ? ZY_COMPOSE_DIRTY : reverse ? ZY_COMPOSE_REVERSE : ZY_COMPOSE;
    int completed=zy_sc_rpc(opcode,payload,5+count*12);
    GPU_ADD(rpc_start,zy_gpu_rpc_ticks);
    if(!completed) {fallback();return;}
    /* Consume the tiny hit bitmap before the Blitter starts streaming the
     * framebuffer from the same cartridge window. */
    for(unsigned i=0;i<count;i++) if(records[i].collide && (cart(ZY_HITS_OFFSET+(i/16)*2)&(1u<<(i%16))))
        frame_image[records[i].hit]=0xff;
    GPU_TIME(copy_start);
#ifdef ZY_SC_TEST
    extern uint16_t *zy_test_cart(unsigned);
    if(dirty_mode)zy_sc_copy_rects_blitter(zy_test_cart(ZY_OUTPUT_OFFSET),frame_image+screen,payload+5,count);
    else if(reverse) {
#ifdef ZY_ASYNC_BLITTER
        zy_sc_copy_scene_blitter_start(zy_test_cart(ZY_OUTPUT_OFFSET),frame_image+screen);
        copy_pending=1;
#ifdef ZY_PROFILE
        copy_started=copy_start;
#endif
#else
        zy_sc_copy_scene_blitter(zy_test_cart(ZY_OUTPUT_OFFSET),frame_image+screen);
#endif
    }
    else zy_sc_copy_scene(zy_test_cart(ZY_OUTPUT_OFFSET),frame_image+screen);
#else
    if(dirty_mode)zy_sc_copy_rects_blitter((const volatile void *)(uintptr_t)(0xfa0000UL+ZY_OUTPUT_OFFSET),frame_image+screen,payload+5,count);
    else if(reverse) {
#ifdef ZY_ASYNC_BLITTER
        zy_sc_copy_scene_blitter_start((const volatile void *)(uintptr_t)(0xfa0000UL+ZY_OUTPUT_OFFSET),frame_image+screen);
        copy_pending=1;
#ifdef ZY_PROFILE
        copy_started=copy_start;
#endif
#else
        zy_sc_copy_scene_blitter((const volatile void *)(uintptr_t)(0xfa0000UL+ZY_OUTPUT_OFFSET),frame_image+screen);
#endif
    }
    else zy_sc_copy_scene((const volatile void *)(uintptr_t)(0xfa0000UL+ZY_OUTPUT_OFFSET),frame_image+screen);
#endif
#ifdef ZY_ASYNC_BLITTER
    if(!reverse)GPU_ADD(copy_start,zy_gpu_copy_ticks);
#else
    GPU_ADD(copy_start,zy_gpu_copy_ticks);
#endif
    zy_gpu_frames++;zy_gpu_sprites+=count;collecting=0;
}
#endif
