/* Target-only backend. Linker --wrap intercepts the upstream scroll ASM seams.
 * The original cores and their assembly remain unchanged. */
#include "machine.h"
#include <string.h>
#include "next_features.h"
#include "terrain.h"
#ifdef ZY_FIX_TERRAIN
int zy_terrain_enabled;
#endif
#include "scroll.h"
#include "init.h"
#include "video.h"
#include "scroll_cache.h"
#include "protocol.h"
#include "tile_decode.h"
#include "entity.h"
#include "enemy.h"
#include "player.h"
#include "sprite.h"
#include "full_gpu.h"
static struct { uint32_t map; uint8_t shift; } cells[ZY_PAGES][ZY_COLUMNS];
static uint32_t current_map;
static unsigned current_shift;
static uint8_t current_tiles[72];
static uint8_t valid[ZY_PAGES][ZY_COLUMNS];
static uint8_t packet[7];
static uint16_t sequence;
static int active, loaded, sparse_supported;
enum { FAIL_CACHE=1, FAIL_PAGE, FAIL_EMIT_CACHE, FAIL_LOAD_RPC, FAIL_EMIT_RPC, FAIL_DRAW_RPC };
volatile uint32_t zy_sc_failure_reason, zy_sc_timeouts;
volatile uint32_t zy_sc_frames, zy_sc_fallbacks, zy_sc_upload_bytes;
#if defined(ZY_PROFILE) && !defined(ZY_SC_TEST)
extern volatile uint32_t zy_vbl_ticks;
static uint32_t profile_frames, profile_vbl, profile_clock;
static uint32_t previous_vbl, cadence[16],worst_vbl;
static uint32_t rpc_ticks, cache_ticks, copy_ticks;
static void debug_byte(unsigned c) {
    volatile uint8_t sink=*(volatile uint8_t *)(uintptr_t)(0xfbff00UL+c);
    unsigned delay;
    (void)sink;
    for(delay=0;delay<100;delay++) __asm__ volatile("nop");
}
static void debug_word(uint32_t value) {
    int shift;
    for(shift=28;shift>=0;shift-=4) {
        unsigned digit=(value>>shift)&15;
        debug_byte(digit<10 ? '0'+digit : 'A'+digit-10);
    }
    debug_byte(' ');
}
static void debug_event(unsigned event, uint32_t reason) {
    debug_byte(event); debug_byte(' ');
    debug_word(reason); debug_word(sequence);
    debug_word(*(volatile uint16_t *)(uintptr_t)(0xfa0000UL+ZY_ACK_OFFSET));
    debug_word(*(volatile uint16_t *)(uintptr_t)(0xfa0000UL+ZY_STATUS_OFFSET));
    debug_word(zy_sc_frames); debug_byte('\n');
}
#define MARK_TIME(name) uint32_t name=clock_200hz()
#define ADD_TIME(name,accumulator) accumulator+=clock_200hz()-name
#else
#define MARK_TIME(name) ((void)0)
#define ADD_TIME(name,accumulator) ((void)0)
#define debug_event(event,reason) ((void)0)
#endif
extern void zy_sc_copy_window(const volatile void *source, void *destination);
extern void zy_sc_copy_sparse(const volatile void *source, void *destination);
extern int zy_sc_copy_runs(const volatile void *source, void *destination);
extern void zy_sc_workspace_edge(const void *source,void *destination);
static void workspace_edge(uint8_t *image,uint32_t destination) {
#ifdef ZY_FIX_TERRAIN
    if(zy_terrain_enabled){zy_terrain_native_column(image,current_map,current_shift,destination);return;}
#endif
    zy_sc_workspace_edge(image+A_scroll_col_workspace,image+destination);
}
extern void zy_sc_clear_gap(void *screen);
#ifndef ZY_SC_TEST
extern void zy_sc_copy_edge(const volatile void *source, void *destination);
#endif
#ifdef ZY_SC_TEST
extern uint16_t *zy_test_cart(unsigned offset);
extern void zy_test_send(unsigned address);
#define CART_WORD(off) (*zy_test_cart(off))
static uint32_t clock_200hz(void) { static uint32_t ticks; return ticks++; }
static void send_address(unsigned a) { zy_test_send(a); }
#else
static uint32_t clock_200hz(void) {
    uint32_t value;
    __asm__ volatile("move.l 0x4ba,%0" : "=d"(value) : : "memory");
    return value;
}
#define CART_WORD(off) (*(volatile uint16_t *)(uintptr_t)(0xfa0000UL+(off)))
static void send_address(unsigned a) { (void)*(volatile uint16_t *)(uintptr_t)(0xfb0000UL+a); }
#endif
/* Kept in the ELF for debugger/benchmark inspection. */
#if defined(ZY_PROFILE) && !defined(ZY_SC_TEST)
static uint32_t stage_start, stage_ticks[4], stage_frames, stage_entities, stage_boss_frames, stage_sprite_ticks;
static uint32_t stage_max[4], max_sprite_explosion_ticks, max_sprite_plain_ticks;
static unsigned max_sprite_explosions, max_sprite_entities, max_sprite_count, max_sprite_plain_count;
static unsigned frame_entity_count;
static const uint8_t *stage_image;
#ifdef ZY_FULL_GPU
extern volatile uint32_t zy_gpu_sprites;
static uint32_t frame_gpu_sprite_base;
#endif
void zy_profile_sprites_done(void) { stage_sprite_ticks+=clock_200hz()-stage_start; }
void zy_stage_begin(unsigned stage,const uint8_t *image) {
    unsigned i;
    if(!stage) {
        for(i=0;i<20;i++) stage_entities+=image[A_entity_table+i*ENTITY_STRIDE+ENTITY_ALIVE]!=0;
        stage_boss_frames+=image[A_boss_sequence_active]!=0;
    }
    if(stage==2) {
        frame_entity_count=0;
        for(i=0;i<20;i++) frame_entity_count+=image[A_entity_table+i*ENTITY_STRIDE+ENTITY_ALIVE]!=0;
#ifdef ZY_FULL_GPU
        frame_gpu_sprite_base=zy_gpu_sprites;
#endif
    }
    stage_image=image;
    stage_start=clock_200hz();
}
void zy_stage_end(unsigned stage) {
    uint32_t elapsed=clock_200hz()-stage_start;
    stage_ticks[stage]+=elapsed;
    if(elapsed>stage_max[stage]) {
        stage_max[stage]=elapsed;
    }
    if(stage==2) {
        unsigned active_groups=0;
        /* Enemy deaths replace their entity type with 0x64. The active-group
         * bitmap tracks section/ship blasts, not ordinary enemy explosions. */
        for(unsigned i=0;i<20;i++) {
            const uint8_t *entity=stage_image+A_entity_table+i*ENTITY_STRIDE;
            active_groups+=entity[ENTITY_TYPE]==EXPLOSION_PART_TYPE || entity[ENTITY_TYPE]==0x65u;
        }
#ifdef ZY_FULL_GPU
        unsigned sprite_count=zy_gpu_sprites-frame_gpu_sprite_base;
#else
        unsigned sprite_count=0;
#endif
        if(active_groups) {
            if(elapsed>max_sprite_explosion_ticks) {
                max_sprite_explosion_ticks=elapsed;
                max_sprite_explosions=active_groups;
                max_sprite_entities=frame_entity_count;
                max_sprite_count=sprite_count;
            }
        } else if(elapsed>max_sprite_plain_ticks) {
            max_sprite_plain_ticks=elapsed;
            max_sprite_plain_count=sprite_count;
        }
    }
    if(stage!=3 || ++stage_frames!=64) return;
    debug_byte('W'); debug_byte(' ');
    debug_word(stage_frames);
    { unsigned i; for(i=0;i<4;i++) { debug_word(stage_ticks[i]); stage_ticks[i]=0; } }
    debug_word(stage_entities); debug_word(stage_boss_frames); debug_byte('\n');
    debug_byte('B'); debug_byte(' '); debug_word(stage_sprite_ticks); debug_byte('\n');
    debug_byte('E');debug_byte(' ');
    debug_word(stage_max[1]);debug_word(stage_max[2]);debug_word(stage_max[3]);
    debug_word(max_sprite_explosion_ticks);debug_word(max_sprite_explosions);
    debug_word(max_sprite_entities);debug_word(max_sprite_count);
    debug_word(max_sprite_plain_ticks);debug_word(max_sprite_plain_count);
    debug_byte('\n');
#ifdef ZY_FULL_GPU
    {
        extern uint32_t zy_gpu_queue_ticks,zy_gpu_rpc_ticks,zy_gpu_copy_ticks;
        extern uint32_t zy_gpu_overlap_ticks,zy_gpu_wait_ticks,zy_gpu_busy_frames;
        extern volatile uint32_t zy_gpu_frames,zy_gpu_fallbacks;
        debug_byte('G');debug_byte(' ');debug_word(stage_frames);
        debug_word(zy_gpu_queue_ticks);debug_word(zy_gpu_rpc_ticks);debug_word(zy_gpu_copy_ticks);
        debug_word(zy_gpu_overlap_ticks);debug_word(zy_gpu_wait_ticks);debug_word(zy_gpu_busy_frames);
        debug_word(zy_gpu_frames);debug_word(zy_gpu_fallbacks);debug_byte('\n');
        zy_gpu_queue_ticks=zy_gpu_rpc_ticks=zy_gpu_copy_ticks=0;
        zy_gpu_overlap_ticks=zy_gpu_wait_ticks=zy_gpu_busy_frames=0;
    }
#endif
    stage_sprite_ticks=0;
    stage_frames=stage_entities=stage_boss_frames=0;
    for(unsigned i=0;i<4;i++)stage_max[i]=0;
    max_sprite_explosion_ticks=max_sprite_plain_ticks=0;
    max_sprite_explosions=max_sprite_entities=max_sprite_count=max_sprite_plain_count=0;
}
#endif
/* Packet words start at odd byte offsets. GCC may combine ordinary byte
 * stores into MOVE.W even with -fno-store-merging; force byte bus cycles. */
static void packet_word(unsigned offset, unsigned value) {
    volatile uint8_t *p=packet+offset;
    p[0]=(uint8_t)(value>>8);
    p[1]=(uint8_t)value;
}
static int rpc(unsigned op, const uint8_t *data, unsigned size) {
    unsigned i,crc,a=0,b=0,polls=0;
    uint32_t start;
    MARK_TIME(profile_start);
    sequence++; if(!sequence) sequence++;
    packet[0]=(uint8_t)op; packet_word(1,sequence); packet_word(3,size);
    zy_fletcher_update(packet,5,&a,&b);
    zy_fletcher_update(data,size,&a,&b);
    crc=(b<<8)|a; packet_word(5,crc);
    send_address(ZY_BUS_START);
    for(i=0;i<5;i++) send_address(ZY_BUS_DATA+2u*packet[i]);
    for(i=0;i<size;i++) send_address(ZY_BUS_DATA+2u*data[i]);
    send_address(ZY_BUS_DATA+2u*packet[5]);
    send_address(ZY_BUS_DATA+2u*packet[6]);
    send_address(ZY_BUS_END);
    /* 200 Hz TOS clock continues through the game's VBL replacement. */
    start=clock_200hz();
    while(CART_WORD(ZY_ACK_OFFSET)!=sequence) {
        if((uint32_t)(clock_200hz()-start)>100 || ++polls==32768) {
            zy_sc_timeouts++;
            debug_event('T',op);
            ADD_TIME(profile_start,rpc_ticks);
            return 0;
        }
    }
    ADD_TIME(profile_start,rpc_ticks);
    zy_sc_upload_bytes+=size+7;
    return CART_WORD(ZY_STATUS_OFFSET)==0;
}
void zy_store_begin(void) {
    if(CART_WORD(ZY_STORE_CAPS_OFFSET)!=ZY_STORE_CAPS_MAGIC ||
       CART_WORD(ZY_STORE_CAPS_OFFSET+2)!=(uint16_t)~ZY_STORE_CAPS_MAGIC)return;
    sequence=CART_WORD(ZY_ACK_OFFSET);
    /* Enter the firmware's low-latency polling path before first-run SD I/O. */
    (void)rpc(ZY_RESET,0,0);
}
int zy_store_attach(void) {
    extern void zy_console(const char *);
    if(CART_WORD(ZY_STORE_CAPS_OFFSET)!=ZY_STORE_CAPS_MAGIC ||
       CART_WORD(ZY_STORE_CAPS_OFFSET+2)!=(uint16_t)~ZY_STORE_CAPS_MAGIC)return 1;
    sequence=CART_WORD(ZY_ACK_OFFSET);
    if(!rpc(ZY_CACHE_COMMIT,0,0))return 0;
    zy_console("SidecarT: preparing local graphics cache...\r\n");
    uint32_t start=clock_200hz();
    while(CART_WORD(ZY_STORE_STATUS_OFFSET)==ZY_STORE_BUSY) {
        if((uint32_t)(clock_200hz()-start)>24000u)return 0;
    }
    if(CART_WORD(ZY_STORE_STATUS_OFFSET)!=ZY_STORE_READY){
        zy_console("SidecarT cache unavailable. Check SD and retry.\r\n");return 0;
    }
    return 1;
}
#ifdef ZY_FULL_GPU
int zy_sc_rpc(unsigned op,const uint8_t *data,unsigned size) {return rpc(op,data,size);}
int zy_sc_is_active(void) {return active;}
#endif
static int page_index(uint8_t *image, uint32_t address, unsigned *column) {
    unsigned p;
    for(p=0;p<ZY_PAGES;p++) {
        uint32_t base=be32(image+A_map_page_table+4*p);
        if(address>=base && address-base<160 && !((address-base)&7)) {
            *column=(unsigned)(address-base)/8; return (int)p;
        }
    }
    return -1;
}
static void restore_workspace(uint8_t *image,const uint8_t *tiles,unsigned shift) {
    unsigned i;
    zy_decode_tiles(image+A_tile_set_base,39488,tiles,image+A_scroll_col_workspace);
    if(shift) for(i=0;i<ZY_MAX_PAYLOAD;i+=4) {
        uint32_t word=be32(image+A_scroll_col_workspace+i);
        wr32(image+A_scroll_col_workspace+i,shift<32 ? word<<shift : 0);
    }
}
static void restore_cell_workspace(uint8_t *image,uint32_t map,unsigned shift) {
#ifdef ZY_FIX_TERRAIN
    if(zy_terrain_enabled && map>=A_map_unpacked+400u*36u) {
        memset(image+A_scroll_col_workspace,0,ZY_MAX_PAYLOAD);return;
    }
#endif
    restore_workspace(image,image+map,shift);
}
static void recover(uint8_t *image, unsigned reason) {
    unsigned p,x,y,b;
    zy_gpu_abort();
    /* Retain only map references during play. Rebuild pixels solely on failure. */
    for(p=0;p<ZY_PAGES;p++) {
        uint32_t base=be32(image+A_map_page_table+4*p);
        for(x=0;x<ZY_COLUMNS;x++) if(valid[p][x]) {
            restore_cell_workspace(image,cells[p][x].map,cells[p][x].shift);
            for(y=0;y<ZY_ROWS;y++) for(b=0;b<4;b++)
                wr16(image+base+y*160+x*8+b*2,be16(image+A_scroll_col_workspace+y*16+b*4));
        }
    }
    if(loaded) restore_workspace(image,current_tiles,current_shift);
    active=0; zy_sc_fallbacks++; zy_sc_failure_reason=reason;
    debug_event('F',reason);
}
int zy_sc_draw_native_layout(uint8_t *image,uint32_t page,uint32_t screen,unsigned phase) {
#ifdef ZY_FIX_TERRAIN
    unsigned column;
    int p=page_index(image,page,&column);
    if(!zy_terrain_enabled || p<0 || column || phase>=20)return 0;
    column=phase+1;if(column==20)column=0;
    for(unsigned x=0;x<20;x++) {
        if(valid[p][column])zy_terrain_native_column(image,cells[p][column].map,cells[p][column].shift,screen+x*8);
        else for(unsigned y=0;y<192;y++)for(unsigned b=0;b<4;b++)wr16(image+screen+x*8+y*160+b*2,0);
        if(++column==20)column=0;
    }
    return 1;
#else
    (void)image;(void)page;(void)screen;(void)phase;return 0;
#endif
}
#ifdef ZY_FIX_TERRAIN
static int layout_tiles(uint8_t *image,uint32_t map,unsigned shift) {
    uint8_t args[82];unsigned variant=image[A_filename_zyn_dat+3];
    args[0]=(uint8_t)(variant=='1'?0:variant=='3'?1:variant=='8'?2:3);
    args[1]=(uint8_t)shift;
    if(args[0]==3 || !zy_terrain_metadata(image,map,args+2))return 0;
    for(unsigned i=0;i<72;i++)args[10+i]=map>=A_map_unpacked+400u*36u?0:image[map+i];
    return rpc(ZY_TILES_LAYOUT,args,sizeof args);
}
static void native_cell(uint8_t *image,uint32_t page) {
    unsigned x;int p=page_index(image,page,&x);
    if(p>=0){cells[p][x].map=current_map;cells[p][x].shift=(uint8_t)current_shift;valid[p][x]=1;}
}
#endif
#ifdef ZY_FULL_GPU
void zy_sc_gpu_failed(uint8_t *image) {if(active)recover(image,FAIL_DRAW_RPC);}
#endif
void __real_section_start_prefill(uint8_t *image);
void __wrap_section_start_prefill(uint8_t *image) {
    unsigned p,x;
    /* A life can restart on pages 1..7. Preserve the shifted workspace before
     * resetting the cache: prefill may emit it before decoding page zero. */
    if(active && loaded) restore_workspace(image,current_tiles,current_shift);
    active=0; loaded=0;
    zy_gpu_reset();
#ifdef ZY_FIX_TERRAIN
    {
        uint8_t metadata[8];
        zy_terrain_enabled=CART_WORD(ZY_TERRAIN_CAPS_OFFSET)==ZY_TERRAIN_CAPS_MAGIC &&
            CART_WORD(ZY_TERRAIN_CAPS_OFFSET+2)==(uint16_t)~ZY_TERRAIN_CAPS_MAGIC &&
            zy_terrain_metadata(image,A_map_unpacked,metadata);
    }
#endif
    sequence=CART_WORD(ZY_ACK_OFFSET);
#ifdef ZY_FULL_GPU
    if(image[A_asteroid_section_flag] && CART_WORD(ZY_FULL_CAPS_OFFSET)==ZY_FULL_CAPS_MAGIC)
        (void)rpc(ZY_RESET,0,0);
#endif
#if defined(ZY_SPARSE_EXPERIMENTAL) || defined(ZY_SC_TEST)
    sparse_supported=CART_WORD(ZY_CAPS_OFFSET)==ZY_CAPS_MAGIC &&
        CART_WORD(ZY_CAPS_OFFSET+2)==(uint16_t)~ZY_CAPS_MAGIC;
#else
    /* Hardware measurements rejected this path: fewer bytes cost more CPU.
     * Keep the original GPU DRAW protocol as the normal build default. */
    sparse_supported=0;
#endif
    if(
#ifdef ZY_GPU_DISABLE
       0 &&
#endif
       !image[A_asteroid_section_flag] && CART_WORD(ZY_MAGIC_OFFSET)==ZY_MAGIC &&
       CART_WORD(ZY_VERSION_OFFSET)==ZY_VERSION) {
        sequence=CART_WORD(ZY_ACK_OFFSET);
        if(rpc(ZY_RESET,0,0)) {
            for(p=0;p<ZY_PAGES;p++) for(x=0;x<ZY_COLUMNS;x++) valid[p][x]=0;
            active=1;
        }
    }
    debug_event('S',active);
    __real_section_start_prefill(image);
}
static int emit(uint8_t *image,uint32_t page,uint32_t edge,unsigned shift) {
    unsigned x;
    int p=page_index(image,page,&x);
    uint8_t args[3];
    if(p<0) { recover(image,FAIL_PAGE); return 0; }
    if(!loaded) {
        loaded=1;
        if(!(
#ifdef ZY_FIX_TERRAIN
             zy_terrain_enabled ? layout_tiles(image,current_map,current_shift) :
#endif
             rpc(ZY_LOAD,image+A_scroll_col_workspace,ZY_MAX_PAYLOAD))) {
            recover(image,FAIL_LOAD_RPC); return 0;
        }
    }
    if(current_shift<32) current_shift+=shift;
    cells[p][x].map=current_map; cells[p][x].shift=(uint8_t)current_shift;
    valid[p][x]=1;
    args[0]=(uint8_t)p; args[1]=(uint8_t)x; args[2]=(uint8_t)shift;
    if(!rpc(ZY_EMIT,args,3)) {
        /* Mirror already contains this emit: recovery performs it exactly once. */
        recover(image,FAIL_EMIT_RPC); return 1;
    }
    if(!image[A_scroll_prefill_hide_screen] && !zy_gpu_collecting()) {
        volatile uint16_t *src=
#ifdef ZY_SC_TEST
            zy_test_cart(ZY_OUTPUT_OFFSET);
#else
            (volatile uint16_t *)(uintptr_t)(0xfa0000UL+ZY_OUTPUT_OFFSET);
#endif
#ifdef ZY_SC_TEST
        unsigned y,b;
        for(y=0;y<ZY_OUTPUT_ROWS;y++) for(b=0;b<4;b++)
            wr16(image+edge+y*160+b*2,*src++);
#else
        zy_sc_copy_edge(src,image+edge);
#endif
    }
    return 1;
}
uint32_t __real_scroll_emit_tile_column_asm(uint8_t *,uint32_t,uint32_t,uint32_t);
static uint32_t native_tiles(uint8_t *image,uint32_t edge,uint32_t page,uint32_t map) {
    uint8_t hide=image[A_scroll_prefill_hide_screen];
    uint32_t result;
#ifdef ZY_FIX_TERRAIN
    if(zy_terrain_enabled){current_map=map;current_shift=0;native_cell(image,page);}
    if(zy_terrain_enabled && map>=A_map_unpacked+400u*36u) {
        memset(image+A_scroll_col_workspace,0,ZY_MAX_PAYLOAD);
        for(unsigned y=0;y<ZY_ROWS;y++)memset(image+page+y*160,0,8);
        if(!hide)workspace_edge(image,edge);
        return map+36;
    }
#endif
    image[A_scroll_prefill_hide_screen]=1;
    result=__real_scroll_emit_tile_column_asm(image,edge,page,map);
    image[A_scroll_prefill_hide_screen]=hide;
    if(!hide) workspace_edge(image,edge);
    return result;
}
uint32_t __wrap_scroll_emit_tile_column_asm(uint8_t *image,uint32_t edge,uint32_t page,uint32_t map) {
    unsigned i,bank,variant=image[A_filename_zyn_dat+3];
    uint8_t args[73];
    if(!active) return native_tiles(image,edge,page,map);
    bank=variant=='1' ? 0 : variant=='3' ? 1 : variant=='8' ? 2 : 3;
    if(bank==3) { recover(image,FAIL_CACHE); return native_tiles(image,edge,page,map); }
    args[0]=(uint8_t)bank;
    current_map=map; current_shift=0; loaded=1;
    for(i=0;i<72;i++) args[1+i]=current_tiles[i]=
#ifdef ZY_FIX_TERRAIN
        zy_terrain_enabled && map>=A_map_unpacked+400u*36u ? 0 :
#endif
        image[map+i];
    if(!(
#ifdef ZY_FIX_TERRAIN
         zy_terrain_enabled ? layout_tiles(image,map,0) :
#endif
         rpc(ZY_TILES,args,sizeof(args)))) {
        recover(image,FAIL_LOAD_RPC);
        return native_tiles(image,edge,page,map);
    }
    (void)emit(image,page,edge,0);
    if(active && zy_gpu_native_background())
        return native_tiles(image,edge,page,map);
    if(!active && !image[A_scroll_prefill_hide_screen])
        workspace_edge(image,edge);
    return map+36;
}
void __real_scroll_emit_column_shift2_asm(uint8_t *,uint32_t,uint32_t,uint32_t);
void __real_scroll_emit_column_shift0_asm(uint8_t *,uint32_t,uint32_t,uint32_t);
static void shift_column(uint8_t *image,uint32_t workspace,uint32_t page,uint32_t edge,unsigned shift) {
#ifdef ZY_FIX_TERRAIN
    if(!active && zy_terrain_enabled){if(current_shift<32)current_shift+=shift;native_cell(image,page);}
#endif
    if(active && emit(image,page,edge,shift)) {
        if(active && zy_gpu_native_background()) {
            uint8_t hide=image[A_scroll_prefill_hide_screen];
            image[A_scroll_prefill_hide_screen]=1;
            if(shift)__real_scroll_emit_column_shift2_asm(image,workspace,page,edge);
            else __real_scroll_emit_column_shift0_asm(image,workspace,page,edge);
            image[A_scroll_prefill_hide_screen]=hide;
            if(!hide)workspace_edge(image,edge);
            return;
        }
        /* Failure recovery also needs the right edge for this operation. */
        if(!active && !image[A_scroll_prefill_hide_screen]) {
            workspace_edge(image,edge);
        }
        return;
    }
    {
        uint8_t hide=image[A_scroll_prefill_hide_screen];
        image[A_scroll_prefill_hide_screen]=1;
        if(shift) __real_scroll_emit_column_shift2_asm(image,workspace,page,edge);
        else __real_scroll_emit_column_shift0_asm(image,workspace,page,edge);
        image[A_scroll_prefill_hide_screen]=hide;
        if(!hide) workspace_edge(image,edge);
    }
}
void __wrap_scroll_emit_column_shift2_asm(uint8_t *i,uint32_t w,uint32_t p,uint32_t e) { shift_column(i,w,p,e,2); }
void __wrap_scroll_emit_column_shift0_asm(uint8_t *i,uint32_t w,uint32_t p,uint32_t e) { shift_column(i,w,p,e,0); }
static int draw(uint8_t *image,uint32_t page,uint32_t screen,unsigned phase) {
    unsigned x;
    int p=page_index(image,page,&x);
    uint8_t args[2];
    {
        int deferred=zy_gpu_defer(image,page,screen,phase);
        if(deferred==1)return 1;
        if(deferred==2)return 0;
    }
    if(p<0 || x!=0) { recover(image,FAIL_PAGE); return 0; }
    args[0]=(uint8_t)p; args[1]=(uint8_t)phase;
    if(!rpc(sparse_supported ? ZY_DRAW_SPARSE : ZY_DRAW,args,2)) { recover(image,FAIL_DRAW_RPC); return 0; }
    MARK_TIME(copy_start);
    {
    const volatile uint16_t *output=
#ifdef ZY_SC_TEST
        zy_test_cart(ZY_OUTPUT_OFFSET);
#else
        (const volatile uint16_t *)(uintptr_t)(0xfa0000UL+ZY_OUTPUT_OFFSET);
#endif
    if(sparse_supported) {
        unsigned dense=*output++;
        if(dense==1) zy_sc_copy_window(output,image+screen);
        else if(dense==2) {
            if(!zy_sc_copy_runs(output,image+screen)) { recover(image,FAIL_DRAW_RPC); return 0; }
        }
        else if(dense==0) zy_sc_copy_sparse(output,image+screen);
        else { recover(image,FAIL_DRAW_RPC); return 0; }
    } else zy_sc_copy_window(output,image+screen);
    }
    ADD_TIME(copy_start,copy_ticks);
    zy_sc_frames++;
    return 1;
}
static void profile_frame(void) {
#if defined(ZY_PROFILE) && !defined(ZY_SC_TEST)
    uint32_t now=zy_vbl_ticks;
    if(!profile_frames) { profile_vbl=now; profile_clock=clock_200hz(); }
    else {
        unsigned elapsed=now-previous_vbl;
        cadence[elapsed<15 ? elapsed : 15]++;
        if(elapsed>worst_vbl)worst_vbl=elapsed;
    }
    previous_vbl=now;
    profile_frames++;
    if(profile_frames==64) {
        debug_byte('P'); debug_byte(' ');
        debug_word(active); debug_word(profile_frames);
        debug_word(zy_vbl_ticks-profile_vbl); debug_word(clock_200hz()-profile_clock);
        debug_word(rpc_ticks); debug_word(cache_ticks); debug_word(copy_ticks);
        debug_word(zy_sc_fallbacks); debug_word(zy_sc_failure_reason);
        debug_byte('\n');
        {
            unsigned i;
            debug_byte('J'); debug_byte(' ');
            for(i=0;i<16;i++) { debug_word(cadence[i]); cadence[i]=0; }
            debug_word(worst_vbl);worst_vbl=0;
            debug_byte('\n');
        }
        profile_frames=0; rpc_ticks=cache_ticks=copy_ticks=0;
    }
#endif
}
#define WRAP_PHASE(nn,n) \
void __real_scroll_page_to_screen_p##nn##_asm(uint8_t *,uint32_t,uint32_t); \
void __wrap_scroll_page_to_screen_p##nn##_asm(uint8_t *i,uint32_t p,uint32_t s) { \
    if(!(active && draw(i,p,s,n)) && !zy_sc_draw_native_layout(i,p,s,n)) { zy_sc_clear_gap(i+s); __real_scroll_page_to_screen_p##nn##_asm(i,p,s); } \
    profile_frame(); \
}
WRAP_PHASE(00,0) WRAP_PHASE(01,1) WRAP_PHASE(02,2) WRAP_PHASE(03,3)
WRAP_PHASE(04,4) WRAP_PHASE(05,5) WRAP_PHASE(06,6) WRAP_PHASE(07,7)
WRAP_PHASE(08,8) WRAP_PHASE(09,9) WRAP_PHASE(10,10) WRAP_PHASE(11,11)
WRAP_PHASE(12,12) WRAP_PHASE(13,13) WRAP_PHASE(14,14) WRAP_PHASE(15,15)
WRAP_PHASE(16,16) WRAP_PHASE(17,17) WRAP_PHASE(18,18) WRAP_PHASE(19,19)
