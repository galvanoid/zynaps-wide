#include "resource_cache.h"
#include "cache_format.h"
#include "sprite_atlas.h"
#include "tile_decode.h"
#include "original_prg.h"
extern const uint32_t zy_original_crc;
const uint8_t *zy_tile_banks[3];
unsigned zy_tile_sizes[3];
static const uint8_t *page_data,*block_data,*directory_data;
static int ready;
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
int zy_resources_ready(void){return ready;}
int zy_resources_bind(const uint8_t *data,unsigned capacity){
    static const unsigned sizes[3]={39488,35648,32128};
    static const uint8_t magic[8]={'Z','Y','W','C','A','C','H','E'};
    ready=0;page_data=block_data=directory_data=0;
    for(unsigned i=0;i<3;i++){zy_tile_banks[i]=0;zy_tile_sizes[i]=0;}
    if(capacity<256 || capacity>ZY_CACHE_CAPACITY)return 0;
    for(unsigned i=0;i<8;i++)if(data[i]!=magic[i])return 0;
    if(be32(data+ZY_H_VERSION)!=1 || be32(data+ZY_H_ORIGINAL_CRC)!=zy_original_crc ||
       be32(data+ZY_H_HEADER_CRC)!=(zy_crc32(0xffffffffu,data,80)^0xffffffffu))return 0;
    unsigned size=be32(data+ZY_H_SIZE),pages=be32(data+ZY_H_PAGES),blocks=be32(data+ZY_H_BLOCKS);
    if(size<256 || size>capacity || !pages || pages>ZY_CACHE_MAX_PAGES || !blocks || blocks>ZY_CACHE_MAX_BLOCKS ||
       be32(data+ZY_H_GROUPS)!=27 || be32(data+ZY_H_PAGES_OFFSET)!=256)return 0;
    uint32_t offset=256u+pages*256u;
    if(be32(data+ZY_H_BLOCKS_OFFSET)!=offset)return 0;
    uint32_t block_offset=offset;offset+=blocks*64u;
    if(be32(data+ZY_H_DIRECTORY_OFFSET)!=offset)return 0;
    uint32_t directory_offset=offset;offset+=80u*27u*2u;
    for(unsigned bank=0;bank<3;bank++){
        if(be32(data+ZY_H_TILE0_OFFSET+bank*8)!=offset || be32(data+ZY_H_TILE0_SIZE+bank*8)!=sizes[bank])return 0;
        offset+=sizes[bank];
    }
    if(offset!=size || be32(data+ZY_H_PAYLOAD_CRC)!=(zy_crc32(0xffffffffu,data+256,size-256)^0xffffffffu))return 0;
    const uint8_t *bp=data+block_offset,*dp=data+directory_offset;
    for(unsigned i=0;i<blocks*32u;i++)if(le16(bp+i*2)>=pages)return 0;
    for(unsigned i=0;i<80u*27u;i++)if(le16(dp+i*2)>=blocks)return 0;
    for(unsigned bank=0;bank<3;bank++){
        zy_tile_banks[bank]=data+be32(data+ZY_H_TILE0_OFFSET+bank*8);zy_tile_sizes[bank]=sizes[bank];
    }
    page_data=data+256;block_data=bp;directory_data=dp;ready=1;return 1;
}
const uint8_t *zy_atlas_page(unsigned scene,uint32_t address){
    if(!ready || scene>=80)return 0;
    unsigned position;
    if(address>=0x19e00u && address<0x19f00u)position=(address-0x19e00u)>>8;
    else if(address>=0x1a800u && address<0x35000u)position=1u+((address-0x1a800u)>>8);
    else if(address>=0x54f00u && address<0x6ec00u)position=425u+((address-0x54f00u)>>8);
    else return 0;
    unsigned block=le16(directory_data+(scene*27u+(position>>5))*2u);
    unsigned id=le16(block_data+block*64u+(position&31u)*2u);
    return page_data+id*256u;
}
