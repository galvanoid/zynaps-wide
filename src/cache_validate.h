#ifndef ZY_CACHE_VALIDATE_H
#define ZY_CACHE_VALIDATE_H
#include <stdint.h>
typedef int (*zy_cache_reader)(void *context,uint32_t at,void *out,unsigned bytes);
/* Validate the complete file before erasing flash: layout, indices and CRC. */
int zy_cache_validate(zy_cache_reader read,void *context,uint32_t bytes);
#endif
