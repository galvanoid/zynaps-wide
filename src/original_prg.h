#ifndef ZY_ORIGINAL_PRG_H
#define ZY_ORIGINAL_PRG_H
#include <stdint.h>
#define ZY_ORIGINAL_TEXT 40774u
uint32_t zy_crc32(uint32_t crc,const uint8_t *bytes,unsigned length);
int zy_original_header(const uint8_t header[28]);
int zy_original_relocate(uint8_t *text,const uint8_t *relocations,unsigned length);
#endif
