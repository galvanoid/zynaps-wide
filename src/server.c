#include "server.h"
#include "tile_decode.h"
#include <string.h>
#ifdef ZY_FULL_GPU
#include "sprite_compositor.h"
#include "app_store.h"
#include "resource_cache.h"
#endif
static unsigned word(const uint8_t *p) { return ((unsigned)p[0]<<8)|p[1]; }
void zy_server_init(zy_server *s, uint16_t *shared) {
    memset(s,0,sizeof(*s)); s->shared=shared;
    shared[ZY_MAGIC_OFFSET/2]=ZY_MAGIC;
    shared[ZY_VERSION_OFFSET/2]=ZY_VERSION;
    shared[ZY_ACK_OFFSET/2]=0;
    shared[ZY_CAPS_OFFSET/2]=0;
    shared[ZY_CAPS_OFFSET/2+1]=0;
    shared[ZY_TERRAIN_CAPS_OFFSET/2]=zy_resources_ready()?ZY_TERRAIN_CAPS_MAGIC:0;
    shared[ZY_TERRAIN_CAPS_OFFSET/2+1]=zy_resources_ready()?(uint16_t)~ZY_TERRAIN_CAPS_MAGIC:0;
#ifdef ZY_FULL_GPU
    shared[ZY_BOSS2X_CAPS_OFFSET/2]=0;
    shared[ZY_BOSS2X_CAPS_OFFSET/2+1]=0;
    shared[ZY_FULL_CAPS_OFFSET/2]=zy_resources_ready()?ZY_FULL_CAPS_MAGIC:0;
    shared[ZY_FULL_CAPS_OFFSET/2+1]=zy_resources_ready()?(uint16_t)~ZY_FULL_CAPS_MAGIC:0;
    shared[ZY_REVERSE_CAPS_OFFSET/2]=zy_resources_ready()?ZY_REVERSE_CAPS_MAGIC:0;
    shared[ZY_REVERSE_CAPS_OFFSET/2+1]=zy_resources_ready()?(uint16_t)~ZY_REVERSE_CAPS_MAGIC:0;
    shared[ZY_DIRTY_CAPS_OFFSET/2]=0;
    shared[ZY_DIRTY_CAPS_OFFSET/2+1]=0;
#endif
}
#ifdef ZY_FULL_GPU
static void reverse_frame_chunks(uint16_t *out) {
    /* Match md-framebuffer-template: reverse the order of 48-byte chunks,
     * preserving the twelve longwords inside every chunk.  The STE Blitter
     * walks destinations backwards one chunk at a time. */
    uint32_t *words=(uint32_t *)out;
    const unsigned longs_per_chunk=12, chunks=(ZY_COMPOSE_WORDS*2)/48;
    for(unsigned first=0;first<chunks/2;first++) {
        unsigned last=chunks-1-first;
        for(unsigned i=0;i<longs_per_chunk;i++) {
            uint32_t value=words[first*longs_per_chunk+i];
            words[first*longs_per_chunk+i]=words[last*longs_per_chunk+i];
            words[last*longs_per_chunk+i]=value;
        }
    }
}
#endif
static void execute(zy_server *s) {
    uint8_t *p=s->packet;
    unsigned n,seq;
    int result=-1;
    if(s->count<7) return;
    n=word(p+3); seq=word(p+1);
    if(n>ZY_MAX_PAYLOAD || s->count!=n+7 || word(p+5+n)!=zy_fletcher(p,5+n)) return;
    switch(p[0]) {
    case ZY_CACHE_COMMIT:
        if(n==0){zy_store_request();result=0;}break;
    case ZY_RESET:
        if(n==0) { zy_cache_reset(&s->cache); s->resets++; result=0; } break;
    case ZY_LOAD:
        if(n==ZY_MAX_PAYLOAD) { result=zy_cache_load(&s->cache,p+5); if(!result) s->loads++; }
        break;
    case ZY_TILES:
        if(n==73 && p[5]<3 && zy_resources_ready()) {
            uint8_t workspace[ZY_MAX_PAYLOAD];
            result=zy_decode_tiles(zy_tile_banks[p[5]],zy_tile_sizes[p[5]],p+6,workspace);
            if(!result) result=zy_cache_load(&s->cache,workspace);
            if(!result) s->loads++;
        }
        break;
    case ZY_TILES_LAYOUT:
        /* bank, current shift, two (move-mask BE24, bridge byte), 72 tile bytes */
        if(n==82 && p[5]<3 && p[6]<=32 && zy_resources_ready()) {
            uint8_t workspace[ZY_MAX_PAYLOAD];
            result=zy_decode_tiles(zy_tile_banks[p[5]],zy_tile_sizes[p[5]],p+15,workspace);
            if(!result)result=zy_cache_load_layout(&s->cache,workspace,p+7);
            if(!result){s->cache.shift=p[6];s->loads++;}
        }
        break;
    case ZY_EMIT:
        if(n==3) {
            result=zy_cache_emit(&s->cache,p[5],p[6],p[7]);
            if(!result) { zy_cache_edge(&s->cache,s->shared+ZY_OUTPUT_OFFSET/2); s->emits++; }
        } break;
    case ZY_DRAW:
        if(n==2 && p[5]<ZY_PAGES && p[6]<ZY_COLUMNS) {
            zy_cache_draw(&s->cache,p[5],p[6],s->shared+ZY_OUTPUT_OFFSET/2); s->draws++; result=0;
        } break;
#ifdef ZY_FULL_GPU
    case ZY_COMPOSE:
    case ZY_COMPOSE_REVERSE:
        if(n>=5 && p[5]<ZY_ATLAS_SCENES && p[6]<ZY_PAGES && p[7]<ZY_COLUMNS && p[8]<=1 &&
           p[9]<=ZY_COMPOSE_MAX_COMMANDS && n==5+p[9]*ZY_COMPOSE_COMMAND_BYTES && zy_resources_ready()) {
            uint16_t *out=s->shared+ZY_OUTPUT_OFFSET/2,hits[6];
            if(p[8])memset(out,0,ZY_COMPOSE_WORDS*2);
            else zy_cache_scene(&s->cache,p[6],p[7],out);
            if(zy_atlas_compose(out,p+10,p[9],p[5],hits)) {
                if(p[0]==ZY_COMPOSE_REVERSE)reverse_frame_chunks(out);
                for(unsigned i=0;i<6;i++)s->shared[ZY_HITS_OFFSET/2+i]=hits[i];
                s->composed_frames++;s->composed_sprites+=p[9];result=0;
            }
        } break;
#endif
    }
    if(result) s->errors++;
    s->shared[ZY_STATUS_OFFSET/2]=(uint16_t)(result ? 1 : 0);
    /* Complete output/status before acknowledging; ST never reads it while rendering. */
    __asm__ volatile("" ::: "memory");
    *(volatile uint16_t *)(s->shared+ZY_ACK_OFFSET/2)=(uint16_t)seq;
}
void zy_server_sample(zy_server *s, uint16_t a) {
    if(a==ZY_BUS_START) { s->count=0; s->receiving=1; return; }
    if(!s->receiving) return;
    if(a==ZY_BUS_END) { execute(s); s->receiving=0; return; }
    if(a>=ZY_BUS_DATA && a<ZY_BUS_DATA+512 && !(a&1)) {
        if(s->count<sizeof(s->packet)) s->packet[s->count++]=(uint8_t)((a-ZY_BUS_DATA)/2);
        else s->receiving=0;
    } else s->receiving=0;
}
