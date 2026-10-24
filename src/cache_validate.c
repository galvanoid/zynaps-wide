#include "cache_validate.h"
#include "cache_format.h"
#include "original_prg.h"
extern const uint32_t zy_original_crc;
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int zy_cache_validate(zy_cache_reader read,void *ctx,uint32_t bytes){
    uint8_t h[256] __attribute__((aligned(2))),scratch[512] __attribute__((aligned(2)));
    static const unsigned sizes[3]={39488,35648,32128};
    static const char magic[]="ZYWCACHE";
    if(bytes<256 || bytes>ZY_CACHE_CAPACITY || !read(ctx,0,h,256))return 0;
    for(unsigned i=0;i<8;i++)if(h[i]!=(uint8_t)magic[i])return 0;
    if(be32(h+ZY_H_VERSION)!=1 || be32(h+ZY_H_SIZE)!=bytes ||
       be32(h+ZY_H_ORIGINAL_CRC)!=zy_original_crc ||
       be32(h+ZY_H_HEADER_CRC)!=(zy_crc32(0xffffffffu,h,80)^0xffffffffu))return 0;
    unsigned pages=be32(h+ZY_H_PAGES),blocks=be32(h+ZY_H_BLOCKS);
    if(!pages || pages>ZY_CACHE_MAX_PAGES || !blocks || blocks>ZY_CACHE_MAX_BLOCKS ||
       be32(h+ZY_H_GROUPS)!=27 || be32(h+ZY_H_PAGES_OFFSET)!=256)return 0;
    uint32_t at=256+pages*256u,block_at=at;
    if(be32(h+ZY_H_BLOCKS_OFFSET)!=at)return 0;
    at+=blocks*64u;uint32_t dir_at=at;
    if(be32(h+ZY_H_DIRECTORY_OFFSET)!=at)return 0;
    at+=80u*27u*2u;
    for(unsigned bank=0;bank<3;bank++){
        if(be32(h+ZY_H_TILE0_OFFSET+8*bank)!=at || be32(h+ZY_H_TILE0_SIZE+8*bank)!=sizes[bank])return 0;
        at+=sizes[bank];
    }
    if(at!=bytes)return 0;
    uint32_t crc=0xffffffffu;
    for(at=256;at<bytes;){
        unsigned n=bytes-at;if(n>sizeof scratch)n=sizeof scratch;
        if(!read(ctx,at,scratch,n))return 0;
        crc=zy_crc32(crc,scratch,n);
        for(unsigned i=0;i<n;i+=2){
            uint32_t pos=at+i;
            if(pos>=block_at && pos<dir_at){
                if(((unsigned)scratch[i]|((unsigned)scratch[i+1]<<8))>=pages)return 0;
            }else if(pos>=dir_at && pos<dir_at+80u*27u*2u){
                if(((unsigned)scratch[i]|((unsigned)scratch[i+1]<<8))>=blocks)return 0;
            }
        }
        at+=n;
    }
    return (crc^0xffffffffu)==be32(h+ZY_H_PAYLOAD_CRC);
}
