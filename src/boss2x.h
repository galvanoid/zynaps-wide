#ifndef ZY_BOSS2X_H
#define ZY_BOSS2X_H
#include <stdint.h>
extern int zy_boss2x_enabled;
int zy_boss2x_command(const uint8_t *,uint32_t,int,uint8_t *,int16_t *,int16_t *);
int zy_boss2x_native(uint8_t *,uint32_t,uint32_t,int);
void zy_boss2x_pairs(uint8_t *);
#endif
