#ifndef ZY_FRAME_METER_H
#define ZY_FRAME_METER_H
#include <stdint.h>
#include "next_features.h"
#ifdef ZY_FIX_TIMING
#define ZY_METER_BUCKETS 6
#define ZY_METER_FIELDS 48
#define ZY_METER_VERSION 3
#else
#define ZY_METER_BUCKETS 4
#define ZY_METER_FIELDS 24
#define ZY_METER_VERSION 1
#endif
void zy_meter_begin(const uint8_t *image);
void zy_meter_end(void);
const uint32_t *zy_meter_report(unsigned *bytes);
void zy_meter_stage_begin(unsigned);
void zy_meter_stage_end(unsigned);
void zy_meter_wait_begin(void);
void zy_meter_wait_end(void);
uint32_t zy_meter_clock(void);
void zy_meter_component_add(unsigned,uint32_t);
void zy_meter_component_begin(unsigned);
void zy_meter_component_end(unsigned);
#endif
