#ifndef ZY_SPRITE_COMPOSITOR_H
#define ZY_SPRITE_COMPOSITOR_H
#include <stdint.h>
int zy_atlas_commands_valid(const uint8_t *commands,unsigned count);
#include "sprite_atlas.h"
#define ZY_COMPOSE_COMMAND_BYTES 12u
#define ZY_COMPOSE_MAX_COMMANDS 96u
#define ZY_COMPOSE_WORDS (192u*80u)
/* Preclipped command: BE32 sprite address, BE16 destination word,
 * byte rows, byte cells (1/2; bit 7 enables pixel collision), BE32 keep mask.
 * Ordered commands use exactly the original rotated, preshifted sprite words. */
typedef uint16_t (*zy_sprite_read)(void *context,uint32_t address,int *valid);
int zy_sprite_commands_valid(const uint8_t *commands,unsigned count,
                            zy_sprite_read read,void *context);
int zy_sprite_compose(uint16_t *screen,const uint8_t *commands,unsigned count,
                      uint16_t hits[6],zy_sprite_read read,void *context);
int zy_atlas_compose(uint16_t *screen,const uint8_t *commands,unsigned count,
                    unsigned scene,uint16_t hits[6]);
#endif
