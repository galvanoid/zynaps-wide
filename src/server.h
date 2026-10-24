#ifndef ZY_SERVER_H
#define ZY_SERVER_H
#include "scroll_cache.h"
#include "protocol.h"
typedef struct {
    zy_cache cache;
    uint8_t packet[ZY_MAX_PAYLOAD+7];
    unsigned count;
    uint16_t *shared;
    int receiving;
    unsigned resets, loads, emits, draws, errors;
    unsigned sparse_draws, sparse_rows, sparse_words;
    unsigned composed_frames, composed_sprites;
} zy_server;
void zy_server_init(zy_server *s, uint16_t *shared);
void zy_server_sample(zy_server *s, uint16_t address);
#endif
