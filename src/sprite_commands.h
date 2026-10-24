#ifndef ZY_SPRITE_COMMANDS_H
#define ZY_SPRITE_COMMANDS_H
#include <stdint.h>
/* 0: fully clipped; 1: command emitted; -1: unsupported record. */
int zy_sprite_command(const uint8_t *image,uint32_t entity,unsigned step,
                      int collide,uint8_t command[12]);
#endif
