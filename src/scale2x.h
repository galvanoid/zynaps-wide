#ifndef ZY_SCALE2X_H
#define ZY_SCALE2X_H
#include "sprite_compositor.h"
#define ZY_BOSS_COMMAND_BYTES 16u
#define ZY_COMMAND_SCALE2 64u
/* Original 16-pixel masked cell -> 32 pixels, nearest neighbour, two rows.
 * Coordinates are signed pixel positions; clipping happens after enlargement.
 * Returns the pixel-hit flag, or -1 for invalid input. No frame-sized scratch. */
int zy_scale2x_draw(void *screen,int big_endian,uint32_t source,int x,int y,
                    unsigned rows,int collide,zy_sprite_read read,void *context);
int zy_boss_compose(uint16_t *screen,const uint8_t *commands,unsigned count,
                    unsigned scene,uint16_t hits[6]);
#endif
