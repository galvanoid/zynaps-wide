#ifndef ZY_MAP_PROJECTION_H
#define ZY_MAP_PROJECTION_H
#include <stdint.h>
/* Input is the original row-major RLE map. Output is our per-column layout. */
int zy_project_map(const uint8_t *data,unsigned length,uint32_t output[400]);
#endif
