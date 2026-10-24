/* Compact, lossless representation of Zynaps' eight preshifted scroll pages. */
#ifndef ZY_SCROLL_CACHE_H
#define ZY_SCROLL_CACHE_H
#include <stdint.h>
#define ZY_ROWS 144
#define ZY_OUTPUT_ROWS 192
#define ZY_COLUMNS 20
#define ZY_PAGES 8
#define ZY_SLOTS 24
#define ZY_COLUMN_WORDS (ZY_ROWS * 4)
#define ZY_LEGACY_MOVE_MASK 0x3fe00u
typedef struct { uint8_t near_slot, far_slot, shift; } zy_cell;
typedef struct {
    /* Adjacent 32-pixel workspaces share one 16-pixel half. Keep that half
     * once: 27 KB rather than 74 KB, leaving RAM for GEMDRIVE and Wi-Fi. */
    uint16_t column[ZY_SLOTS][ZY_COLUMN_WORDS];
    uint16_t nonzero[ZY_SLOTS][9]; /* conservative occupancy, 144 bits/slot */
    zy_cell cell[ZY_PAGES][ZY_COLUMNS];
    uint8_t current[2], shift;
    uint8_t terrain;
    uint8_t layout[ZY_SLOTS][24]; /* original tile row, 255 blank, 254 bridge */
} zy_cache;
void zy_cache_reset(zy_cache *c);
int zy_cache_load(zy_cache *c, const uint8_t *big_endian_column);
int zy_cache_load_layout(zy_cache *c,const uint8_t *column,const uint8_t metadata[8]);
int zy_cache_emit(zy_cache *c, unsigned page, unsigned column, unsigned shift);
/* Words are in host byte order. Cartridge PIO serves native RP words to the ST. */
void zy_cache_edge(const zy_cache *c, uint16_t *out);
void zy_cache_draw(const zy_cache *c, unsigned page, unsigned phase, uint16_t *out);
void zy_cache_scene(const zy_cache *c,unsigned page,unsigned phase,uint16_t *out);
/* Format word 2: 144 rows, each summing to 19 cells. Command word holds
 * blank-cell count (high byte) / literal-cell count (low), then 4 words/literal.
 * Format word 1: normal packed 192 rows. Dense scenes avoid run overhead.
 * The Atari inserts/clears the central 48 rows. Returns words stored. */
unsigned zy_cache_draw_sparse(const zy_cache *c,unsigned page,unsigned phase,
                              uint16_t *out,unsigned *nonzero_rows);
void zy_cache_page(const zy_cache *c, unsigned page, uint8_t *out);
void zy_cache_workspace(const zy_cache *c, uint8_t *out);
#endif
