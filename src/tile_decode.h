#ifndef ZY_TILE_DECODE_H
#define ZY_TILE_DECODE_H
#include <stdint.h>
extern const uint8_t *zy_tile_banks[3];
extern unsigned zy_tile_sizes[3];
/* Two consecutive column-major map columns, 18 BE16 tile IDs each. */
int zy_decode_tiles(const uint8_t *bank, unsigned bytes, const uint8_t *map,
                    uint8_t *workspace);
#endif
