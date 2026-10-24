#ifndef ZY_RESOURCE_CACHE_H
#define ZY_RESOURCE_CACHE_H
#include <stdint.h>
int zy_resources_bind(const uint8_t *data,unsigned capacity);
int zy_resources_ready(void);
#endif
