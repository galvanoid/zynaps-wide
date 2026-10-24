#ifndef ZY_CACHE_FORMAT_H
#define ZY_CACHE_FORMAT_H
#include <stdint.h>
#define ZY_CACHE_HEADER 256u
#define ZY_CACHE_CAPACITY 450560u
#define ZY_CACHE_FLASH_OFFSET 0x92000u
#define ZY_CACHE_SCENES 80u
#define ZY_CACHE_GROUPS 27u
#define ZY_CACHE_MAX_PAGES 1536u
#define ZY_CACHE_MAX_BLOCKS 256u
/* Header words (big endian); page indices/directories are little endian,
 * allowing the RP2040 to retain native 16-bit lookup loads during play. */
enum {ZY_H_VERSION=8,ZY_H_SIZE=12,ZY_H_PAYLOAD_CRC=16,ZY_H_ORIGINAL_CRC=20,
      ZY_H_PAGES=24,ZY_H_BLOCKS=28,ZY_H_GROUPS=32,ZY_H_PAGES_OFFSET=36,
      ZY_H_BLOCKS_OFFSET=40,ZY_H_DIRECTORY_OFFSET=44,ZY_H_TILE0_OFFSET=48,
      ZY_H_TILE0_SIZE=52,ZY_H_TILE1_OFFSET=56,ZY_H_TILE1_SIZE=60,
      ZY_H_TILE2_OFFSET=64,ZY_H_TILE2_SIZE=68,ZY_H_HEADER_CRC=80};
int zy_cache_builder_begin(void);
int zy_cache_builder_scene(unsigned scene,const uint8_t *image);
int zy_cache_builder_finish(void);
/* Backend callbacks; all are bounded and exact-length, return 1 on success. */
int zy_cache_read(uint32_t offset,void *out,unsigned bytes);
int zy_cache_write(uint32_t offset,const void *data,unsigned bytes);
int zy_cache_tile_read(unsigned bank,uint32_t offset,void *out,unsigned bytes);
#endif
