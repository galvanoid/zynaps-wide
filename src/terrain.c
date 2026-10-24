#include "machine.h"
#include "terrain.h"
#include "scroll.h"
#include "init.h"
#include "tile_decode.h"
#include "wide_geometry.h"

static unsigned move_mask(const uint8_t *metadata) {
    return ((unsigned)metadata[0]<<16)|((unsigned)metadata[1]<<8)|metadata[2];
}
/* Independent direct sampler: source tiles are either kept or moved down six
 * rows. OR allows an empty source tile to share a destination with a moved one.
 * Offline validation rejects any overlap between two nonblank source tiles. */
static uint16_t sample(const uint8_t *pixels,const uint8_t *m,unsigned half,unsigned y,unsigned plane) {
    unsigned mask=move_mask(m),value=0;
    if(y<144 && !(mask&(1u<<(y/8))))value=be16(pixels+y*16+plane*4+half*2);
    if(y>=48 && (mask&(1u<<((y-48)/8))))value|=be16(pixels+(y-48)*16+plane*4+half*2);
    if(m[3] && y>=72 && y<120)
        value|=be16(pixels+71*16+plane*4+half*2)|be16(pixels+72*16+plane*4+half*2);
    return (uint16_t)value;
}
void zy_terrain_native_column(uint8_t *image,uint32_t map,unsigned shift,uint32_t destination) {
    uint8_t pixels[2304],metadata[8];
    /* The visible map ends at 400 columns. The original scroller's lookahead
     * continues through the boss gate; beyond the map, render empty space,
     * never interpret the tile bank at column 420 as map indices. */
    if(map>=A_map_unpacked+400u*36u) {
        for(unsigned y=0;y<192;y++)for(unsigned plane=0;plane<4;plane++)
            wr16(image+destination+y*160+plane*2,0);
        return;
    }
    if(!zy_terrain_metadata(image,map,metadata)) {
        for(unsigned half=0;half<2;half++) {
            metadata[half*4]=3;metadata[half*4+1]=0xfe;
            metadata[half*4+2]=0;metadata[half*4+3]=0;
        }
    }
    if(zy_decode_tiles(image+A_tile_set_base,39488,image+map,pixels))return;
    for(unsigned y=0;y<192;y++)for(unsigned plane=0;plane<4;plane++) {
        uint32_t value=((uint32_t)sample(pixels,metadata,0,y,plane)<<16)|sample(pixels,metadata+4,1,y,plane);
        wr16(image+destination+y*160+plane*2,shift<32?(uint16_t)((value<<shift)>>16):0);
    }
}
uint16_t zy_terrain_ground_y(uint8_t *image,uint32_t script,uint16_t y,unsigned rows) {
    uint8_t metadata[8];
    unsigned map=A_map_unpacked+be16(image+script);
    int contact=(int16_t)y-32+(int)rows;
    int above=(int16_t)y-33;
    if(!zy_terrain_enabled || !zy_terrain_metadata(image,map,metadata))return wide_ground_y(y,rows);
    if(contact>=0 && contact<144 && (be16(image+map+(unsigned)(contact/8)*2)&0x7fff))
        return (uint16_t)(y+((move_mask(metadata)&(1u<<(contact/8)))?48:0));
    if(above>=0 && above<144 && (be16(image+map+(unsigned)(above/8)*2)&0x7fff))
        return (uint16_t)(y+((move_mask(metadata)&(1u<<(above/8)))?48:0));
    return wide_ground_y(y,rows);
}
