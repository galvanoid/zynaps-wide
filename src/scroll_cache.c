#include "scroll_cache.h"
#include <string.h>
static uint32_t shifted(uint32_t v, unsigned n) { return n < 32 ? v << n : 0; }
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static int same_half(const uint16_t *a, const uint16_t *b) {
    unsigned i;
    for(i=0;i<ZY_COLUMN_WORDS;i++) if(a[i]!=b[i]) return 0;
    return 1;
}
void zy_cache_reset(zy_cache *c) {
    memset(c, 0, sizeof(*c));
    memset(c->layout,255,sizeof(c->layout));
    /* Slot zero is an immutable blank until all page references have moved on. */
}
static int cache_load(zy_cache *c,const uint8_t *in,const uint8_t *metadata) {
    uint8_t used[ZY_SLOTS] = {0};
    uint16_t halves[2][ZY_COLUMN_WORDS];
    uint8_t layout[2][24];
    unsigned p,x,s,i,h,chosen[2];
    for(p=0;p<ZY_PAGES;p++) for(x=0;x<ZY_COLUMNS;x++) {
        used[c->cell[p][x].near_slot]=1;
        used[c->cell[p][x].far_slot]=1;
    }
    for(h=0;h<2;h++) for(i=0;i<ZY_COLUMN_WORDS;i++)
#ifdef __m68k__
        /* Workspace is word aligned; avoid byte loads and endian conversion. */
        halves[h][i]=((const uint16_t *)in)[i*2+h];
#else
        halves[h][i]=(uint16_t)(((unsigned)in[i*4+h*2]<<8)|in[i*4+h*2+1]);
#endif
    for(h=0;h<2;h++) {
        unsigned mask=metadata ? ((unsigned)metadata[h*4]<<16)|((unsigned)metadata[h*4+1]<<8)|metadata[h*4+2] : ZY_LEGACY_MOVE_MASK;
        unsigned bridge=metadata ? metadata[h*4+3] : 0;
        if(mask>=0x40000u || bridge>1)return -1;
        memset(layout[h],255,24);
        for(unsigned tile=0;tile<18;tile++) {
            unsigned nonzero=0,destination=tile+((mask&(1u<<tile))?6:0);
            for(unsigned word=tile*32;word<(tile+1)*32;word++)nonzero|=halves[h][word];
            if(!nonzero)continue;
            if(layout[h][destination]!=255)return -1;
            layout[h][destination]=(uint8_t)tile;
        }
        if(bridge)for(unsigned tile=9;tile<15;tile++) {
            if(layout[h][tile]!=255)return -1;
            layout[h][tile]=254;
        }
    }
    for(h=0;h<2;h++) {
        if(h && same_half(halves[0],halves[1]) && !memcmp(layout[0],layout[1],24)) {
            chosen[1]=chosen[0]; continue;
        }
        for(s=0;s<ZY_SLOTS;s++)
            if(used[s] && !(h && s==chosen[0]) &&
               same_half(c->column[s],halves[h]) && !memcmp(c->layout[s],layout[h],24)) break;
        if(s==ZY_SLOTS) {
            for(s=0;s<ZY_SLOTS && used[s];s++) {}
            if(s==ZY_SLOTS) return -1; /* No mutation: ST can restore its mirror. */
        }
        chosen[h]=s; used[s]=1;
    }
    for(h=0;h<2;h++) {
        /* The freestanding Atari memcpy is byte based. These are native words. */
        for(i=0;i<ZY_COLUMN_WORDS;i++) c->column[chosen[h]][i]=halves[h][i];
        memcpy(c->layout[chosen[h]],layout[h],24);
        for(i=0;i<9;i++) c->nonzero[chosen[h]][i]=0;
        for(i=0;i<ZY_ROWS;i++) {
            unsigned at=i*4;
            if(halves[h][at]|halves[h][at+1]|halves[h][at+2]|halves[h][at+3])
                c->nonzero[chosen[h]][i/16]|=(uint16_t)(1u<<(i%16));
        }
        c->current[h]=(uint8_t)chosen[h];
    }
    c->shift=0;
    if(metadata)c->terrain=1;
    return 0;
}
int zy_cache_load(zy_cache *c,const uint8_t *in) {return cache_load(c,in,0);}
int zy_cache_load_layout(zy_cache *c,const uint8_t *in,const uint8_t metadata[8]) {
    return cache_load(c,in,metadata);
}
static uint16_t terrain_word(const zy_cache *c,unsigned slot,unsigned y,unsigned plane) {
    unsigned tile=c->layout[slot][y/8];
    if(tile==255)return 0;
    if(tile==254)return c->column[slot][71*4+plane]|c->column[slot][72*4+plane];
    return c->column[slot][(tile*8+(y&7))*4+plane];
}
static const uint16_t *terrain_tile(const zy_cache *c,unsigned slot,unsigned tile,uint16_t scratch[32]) {
    static const uint16_t blank[32]={0};
    if(tile==255)return blank;
    if(tile!=254)return c->column[slot]+tile*32;
    for(unsigned r=0;r<8;r++)for(unsigned b=0;b<4;b++)
        scratch[r*4+b]=c->column[slot][71*4+b]|c->column[slot][72*4+b];
    return scratch;
}
static void terrain_draw(const zy_cache *c,unsigned p,unsigned phase,uint16_t *out,unsigned columns,unsigned stride) {
    unsigned column=(phase+1)%ZY_COLUMNS;
    memset(out,0,ZY_OUTPUT_ROWS*stride*2);
    for(unsigned x=0;x<columns;x++) {
        zy_cell cell=c->cell[p][column];
        for(unsigned tile=0;tile<24;tile++) {
            unsigned near_id=c->layout[cell.near_slot][tile],far_id=c->layout[cell.far_slot][tile];
            uint16_t near_bridge[32],far_bridge[32];
            if(near_id==255 && far_id==255)continue;
            const uint16_t *near=terrain_tile(c,cell.near_slot,near_id,near_bridge);
            const uint16_t *far=terrain_tile(c,cell.far_slot,far_id,far_bridge);
            for(unsigned row=0;row<8;row++)for(unsigned b=0;b<4;b++) {
                uint32_t value=((uint32_t)*near++<<16)|*far++;
                out[(tile*8+row)*stride+x*4+b]=(uint16_t)(shifted(value,cell.shift)>>16);
            }
        }
        if(++column==ZY_COLUMNS)column=0;
    }
}
int zy_cache_emit(zy_cache *c, unsigned p, unsigned x, unsigned shift) {
    if(p>=ZY_PAGES || x>=ZY_COLUMNS || (shift!=0 && shift!=2)) return -1;
    if(c->shift<32) c->shift=(uint8_t)(c->shift+shift);
    c->cell[p][x].near_slot=c->current[0];
    c->cell[p][x].far_slot=c->current[1];
    c->cell[p][x].shift=c->shift;
    return 0;
}
static uint16_t pixel_word(const zy_cache *c, unsigned p, unsigned x, unsigned i) {
    zy_cell cell=c->cell[p][x];
    uint32_t v=((uint32_t)c->column[cell.near_slot][i]<<16)|c->column[cell.far_slot][i];
    return (uint16_t)(shifted(v,cell.shift)>>16);
}
void zy_cache_edge(const zy_cache *c, uint16_t *out) {
    if(c->terrain) {
        for(unsigned y=0;y<ZY_OUTPUT_ROWS;y++)for(unsigned b=0;b<4;b++) {
            uint32_t v=((uint32_t)terrain_word(c,c->current[0],y,b)<<16)|terrain_word(c,c->current[1],y,b);
            out[y*4+b]=(uint16_t)(shifted(v,c->shift)>>16);
        }
        return;
    }
    unsigned i;
    memset(out+72*4,0,48*8);
    for(i=0;i<ZY_COLUMN_WORDS;i++) {
        uint32_t v=((uint32_t)c->column[c->current[0]][i]<<16)|c->column[c->current[1]][i];
        out[i+(i>=72*4 ? 48*4 : 0)]=(uint16_t)(shifted(v,c->shift)>>16);
    }
}
void zy_cache_draw(const zy_cache *c, unsigned p, unsigned phase, uint16_t *out) {
    if(c->terrain){terrain_draw(c,p,phase,out,19,76);return;}
    unsigned y,x,b,column=(phase+1)%ZY_COLUMNS;
    memset(out+72*76,0,48*152);
    for(x=0;x<19;x++) {
        zy_cell cell=c->cell[p][column];
        const uint16_t *near=c->column[cell.near_slot], *far=c->column[cell.far_slot];
        uint16_t *destination=out+x*4;
        for(y=0;y<ZY_ROWS;y++,destination+=76) {
            if(y==72) destination+=48*76;
            for(b=0;b<4;b++) {
                uint32_t pixels=((uint32_t)*near++<<16)|*far++;
                destination[b]=(uint16_t)(shifted(pixels,cell.shift)>>16);
            }
        }
        if(++column==ZY_COLUMNS) column=0;
    }
}
void zy_cache_page(const zy_cache *c, unsigned p, uint8_t *out) {
    unsigned y,x,b;
    for(y=0;y<ZY_ROWS;y++) for(x=0;x<ZY_COLUMNS;x++) for(b=0;b<4;b++) {
        put16(out,pixel_word(c,p,x,y*4+b)); out+=2;
    }
}
void zy_cache_scene(const zy_cache *c,unsigned p,unsigned phase,uint16_t *out) {
    if(c->terrain){terrain_draw(c,p,phase,out,20,80);return;}
    memset(out+72*80,0,48*160);
    unsigned column=(phase+1)%ZY_COLUMNS;
    for(unsigned x=0;x<20;x++) {
        zy_cell cell=c->cell[p][column];
        const uint16_t *near=c->column[cell.near_slot],*far=c->column[cell.far_slot];
        uint16_t *destination=out+x*4;
        for(unsigned y=0;y<ZY_ROWS;y++,destination+=80) {
            if(y==72)destination+=48*80;
            for(unsigned b=0;b<4;b++) {
                uint32_t pixels=((uint32_t)*near++<<16)|*far++;
                destination[b]=(uint16_t)(shifted(pixels,cell.shift)>>16);
            }
        }
        if(++column==ZY_COLUMNS)column=0;
    }
}
unsigned zy_cache_draw_sparse(const zy_cache *c,unsigned p,unsigned phase,
                              uint16_t *out,unsigned *nonzero_rows) {
    if(c->terrain){out[0]=1;zy_cache_draw(c,p,phase,out+1);*nonzero_rows=192;return 1+192*76;}
    uint16_t row[76], possible[9]={0}, *start=out;
    unsigned y,x,b,occupied=0,cells=0,runs=0,column=(phase+1)%ZY_COLUMNS;
    for(x=0;x<19;x++) {
        zy_cell cell=c->cell[p][column];
        for(b=0;b<9;b++) possible[b]|=c->nonzero[cell.near_slot][b]|c->nonzero[cell.far_slot][b];
        if(++column==ZY_COLUMNS) column=0;
    }
    *out++=2; /* per-row runs of blank/literal 16-pixel cells */
    for(y=0;y<ZY_ROWS;y++) {
        unsigned bits=0,column=(phase+1)%ZY_COLUMNS;
        if(!(possible[y/16]&(1u<<(y%16)))) { *out++=19u<<8; runs++; continue; }
        for(x=0;x<19;x++) {
            zy_cell cell=c->cell[p][column];
            const uint16_t *near=c->column[cell.near_slot]+y*4;
            const uint16_t *far=c->column[cell.far_slot]+y*4;
            for(b=0;b<4;b++) {
                uint32_t raw=((uint32_t)near[b]<<16)|far[b];
                row[x*4+b]=(uint16_t)(shifted(raw,cell.shift)>>16);
            }
            if(row[x*4]|row[x*4+1]|row[x*4+2]|row[x*4+3]) bits|=1u<<x;
            if(++column==ZY_COLUMNS) column=0;
        }
        occupied+=bits!=0;
        x=0;
        while(x<19) {
            unsigned zero=0,literal=0,first;
            while(x<19 && !(bits&(1u<<x))) { zero++; x++; }
            first=x;
            while(x<19 && (bits&(1u<<x))) { literal++; x++; }
            *out++=(uint16_t)((zero<<8)|literal);
            for(b=first*4;b<(first+literal)*4;b++) *out++=row[b];
            cells+=literal; runs++;
        }
    }
    *nonzero_rows=occupied;
    /* Approximate 68000 cost of the unrolled run copier. Fragmented or dense
     * scenes retain the existing MOVEM path instead of paying many commands. */
    if(cells*40u+(ZY_ROWS*19u-cells)*24u+runs*100u>=ZY_ROWS*19u*44u) {
        start[0]=1;
        zy_cache_draw(c,p,phase,start+1);
        return 1+ZY_OUTPUT_ROWS*76;
    }
    return (unsigned)(out-start);
}
void zy_cache_workspace(const zy_cache *c, uint8_t *out) {
    unsigned i;
    for(i=0;i<ZY_COLUMN_WORDS;i++) {
        uint32_t raw=((uint32_t)c->column[c->current[0]][i]<<16)|c->column[c->current[1]][i];
        uint32_t v=shifted(raw,c->shift);
        put16(out,(uint16_t)(v>>16)); put16(out+2,(uint16_t)v); out+=4;
    }
}
