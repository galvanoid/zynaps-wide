#ifndef ZY_TERRAIN_H
#define ZY_TERRAIN_H
#include <stdint.h>
extern int zy_terrain_enabled;
int zy_terrain_metadata(const uint8_t *image,uint32_t map,uint8_t out[8]);
void zy_terrain_native_column(uint8_t *image,uint32_t map,unsigned shift,uint32_t destination);
uint16_t zy_terrain_ground_y(uint8_t *image,uint32_t script,uint16_t y,unsigned rows);
int zy_sc_draw_native_layout(uint8_t *image,uint32_t page,uint32_t screen,unsigned phase);
#endif
