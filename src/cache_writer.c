#include "cache_format.h"
#include "original_prg.h"
extern const uint32_t zy_original_crc;
static uint16_t slots[4096],blocks[ZY_CACHE_MAX_BLOCKS][32],directory[80][27];
static uint32_t hashes[ZY_CACHE_MAX_PAGES];
static unsigned pages,block_count,next_scene,failed;
static uint8_t scratch[512];
static unsigned le16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);return n;}
static void be32(uint8_t *p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static int equal(const uint8_t *a,const uint8_t *b,unsigned n){while(n--)if(*a++!=*b++)return 0;return 1;}
static int fail(void){failed=1;return 0;}
int zy_cache_builder_begin(void){
    for(unsigned i=0;i<4096;i++)slots[i]=0;
    for(unsigned i=0;i<512;i++)scratch[i]=0;
    pages=1;block_count=next_scene=failed=0;
    /* Reserve page zero, allowing exact blank pages to avoid file reads. */
    if(!zy_cache_write(0,scratch,512))return fail();
    return 1;
}
static int page_id(const uint8_t *data){
    uint32_t hash=0x9e3779b9u,nonzero=0;
    for(unsigned i=0;i<256;i+=4){
        uint32_t v=((uint32_t)data[i]<<24)|((uint32_t)data[i+1]<<16)|((uint32_t)data[i+2]<<8)|data[i+3];
        hash=((hash<<5)|(hash>>27))^v;nonzero|=v;
    }
    if(!nonzero)return 0;
    unsigned bucket=hash&4095u;
    for(unsigned probe=0;probe<4096;probe++,bucket=(bucket+1)&4095u){
        if(!slots[bucket]){
            if(pages>=ZY_CACHE_MAX_PAGES || !zy_cache_write(256u+pages*256u,data,256))return -1;
            hashes[pages]=hash;slots[bucket]=(uint16_t)(pages+1);return (int)pages++;
        }
        unsigned id=slots[bucket]-1u;
        if(hashes[id]==hash){
            if(!zy_cache_read(256u+id*256u,scratch,256))return -1;
            if(equal(data,scratch,256))return (int)id;
        }
    }
    return -1;
}
static int block_id(const uint16_t *ids){
    for(unsigned b=0;b<block_count;b++){
        unsigned i=0;while(i<32 && blocks[b][i]==ids[i])i++;
        if(i==32)return (int)b;
    }
    if(block_count>=ZY_CACHE_MAX_BLOCKS)return -1;
    for(unsigned i=0;i<32;i++)blocks[block_count][i]=ids[i];
    return (int)block_count++;
}
int zy_cache_builder_scene(unsigned scene,const uint8_t *image){
    static const uint32_t ranges[3][2]={{0x19e00,0x19f00},{0x1a800,0x35000},{0x54f00,0x6ec00}};
    uint16_t ids[32];unsigned used=0,group=0;
    if(failed || scene!=next_scene || scene>=80)return fail();
    for(unsigned r=0;r<3;r++)for(uint32_t at=ranges[r][0];at<ranges[r][1];at+=256){
        int id=page_id(image+at);if(id<0)return fail();ids[used++]=(uint16_t)id;
        if(used==32){int b=block_id(ids);if(b<0)return fail();directory[scene][group++]=(uint16_t)b;used=0;}
    }
    if(used){
        while(used<32)ids[used++]=0;
        int b=block_id(ids);if(b<0)return fail();directory[scene][group++]=(uint16_t)b;
    }
    if(group!=27)return fail();next_scene++;return 1;
}
int zy_cache_builder_finish(void){
    static const unsigned tile_sizes[3]={39488,35648,32128};
    uint8_t header[256] __attribute__((aligned(2)));
    if(failed || next_scene!=80)return fail();
    for(unsigned i=0;i<256;i++)header[i]=0;
    header[0]='Z';header[1]='Y';header[2]='W';header[3]='C';header[4]='A';header[5]='C';header[6]='H';header[7]='E';
    be32(header+ZY_H_VERSION,1);be32(header+ZY_H_ORIGINAL_CRC,zy_original_crc);
    be32(header+ZY_H_PAGES,pages);be32(header+ZY_H_BLOCKS,block_count);be32(header+ZY_H_GROUPS,27);
    be32(header+ZY_H_PAGES_OFFSET,256);
    uint32_t offset=256u+pages*256u;be32(header+ZY_H_BLOCKS_OFFSET,offset);
    for(unsigned b=0;b<block_count;b++){
        for(unsigned i=0;i<32;i++)le16(scratch+i*2,blocks[b][i]);
        if(!zy_cache_write(offset,scratch,64))return fail();offset+=64;
    }
    be32(header+ZY_H_DIRECTORY_OFFSET,offset);
    for(unsigned scene=0;scene<80;scene++){
        for(unsigned i=0;i<27;i++)le16(scratch+i*2,directory[scene][i]);
        if(!zy_cache_write(offset,scratch,54))return fail();offset+=54;
    }
    for(unsigned bank=0;bank<3;bank++){
        be32(header+ZY_H_TILE0_OFFSET+bank*8,offset);be32(header+ZY_H_TILE0_SIZE+bank*8,tile_sizes[bank]);
        for(unsigned at=0;at<tile_sizes[bank];){
            unsigned n=tile_sizes[bank]-at;if(n>512)n=512;
            if(!zy_cache_tile_read(bank,at,scratch,n))return fail();
            if(!at)for(unsigned i=0;i<32;i++)scratch[i]=0;
            if(offset+n>ZY_CACHE_CAPACITY || !zy_cache_write(offset,scratch,n))return fail();
            offset+=n;at+=n;
        }
    }
    be32(header+ZY_H_SIZE,offset);uint32_t crc=0xffffffffu;
    for(uint32_t at=256;at<offset;){
        unsigned n=offset-at;if(n>512)n=512;
        if(!zy_cache_read(at,scratch,n))return fail();crc=zy_crc32(crc,scratch,n);at+=n;
    }
    be32(header+ZY_H_PAYLOAD_CRC,crc^0xffffffffu);
    be32(header+ZY_H_HEADER_CRC,zy_crc32(0xffffffffu,header,80)^0xffffffffu);
    return zy_cache_write(0,header,256)?1:fail();
}
