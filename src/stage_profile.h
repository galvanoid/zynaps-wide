#ifndef ZY_STAGE_PROFILE_H
#define ZY_STAGE_PROFILE_H
#ifdef ZY_PROFILE
#include <stdint.h>
void zy_stage_begin(unsigned stage,const uint8_t *image);
void zy_stage_end(unsigned stage);
#else
#define zy_stage_begin(stage,image) ((void)0)
#define zy_stage_end(stage) ((void)0)
#endif
#endif
