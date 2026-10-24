#ifndef ZY_PROTOCOL_H
#define ZY_PROTOCOL_H
/* Byte payload encoded as ROM3 reads at $FB6000 + byte*2. */
#define ZY_BUS_DATA 0x6000u
#define ZY_BUS_START 0x6200u
#define ZY_BUS_END 0x6202u
/* After DevOps GEMDRIVE and Runner buffers (last byte $4497). */
#define ZY_MAGIC_OFFSET 0x4500u
#define ZY_VERSION_OFFSET 0x4502u
#define ZY_ACK_OFFSET 0x4504u
#define ZY_STATUS_OFFSET 0x4506u
#define ZY_OUTPUT_OFFSET 0x4508u
#define ZY_MAGIC 0x5a57u
#define ZY_VERSION 3u
#define ZY_RESET 1u
#define ZY_LOAD 2u
#define ZY_EMIT 3u
#define ZY_DRAW 4u
#define ZY_TILES 5u
#define ZY_DRAW_SPARSE 6u
#define ZY_COMPOSE 7u
#define ZY_COMPOSE_REVERSE 8u
#define ZY_COMPOSE_DIRTY 9u
#define ZY_TILES_LAYOUT 11u
#define ZY_COMPOSE_BOSS2X 12u
#define ZY_COMPOSE_BOSS2X_REVERSE 13u
#define ZY_CACHE_COMMIT 20u
#define ZY_STORE_CAPS_OFFSET 0x44ccu
#define ZY_STORE_CAPS_MAGIC 0x4350u
#define ZY_STORE_STATUS_OFFSET 0x44d0u
#define ZY_STORE_MISSING 1u
#define ZY_STORE_BUSY 2u
#define ZY_STORE_READY 3u
#define ZY_STORE_ERROR 4u
#define ZY_BOSS2X_CAPS_OFFSET 0x44c4u
#define ZY_BOSS2X_CAPS_MAGIC 0x4232u
#define ZY_TERRAIN_CAPS_OFFSET 0x44c0u
#define ZY_TERRAIN_CAPS_MAGIC 0x544cu
#define ZY_FULL_CAPS_OFFSET 0x44a4u
#define ZY_FULL_CAPS_MAGIC 0x4655u
#define ZY_REVERSE_CAPS_OFFSET 0x44a8u
#define ZY_REVERSE_CAPS_MAGIC 0x5256u
#define ZY_DIRTY_CAPS_OFFSET 0x44acu
#define ZY_DIRTY_CAPS_MAGIC 0x4452u
#define ZY_HITS_OFFSET 0x44b0u
/* Optional capability, after DevOps buffers and before the v3 header.
 * Keep v3 and DRAW available so existing Wide programs remain compatible. */
#define ZY_CAPS_OFFSET 0x44a0u
#define ZY_CAPS_MAGIC 0x5350u
#define ZY_MAX_PAYLOAD 2304u
/* Packet: opcode, sequence BE16, length BE16, payload, Fletcher16 BE16.
 * START resets the parser; END validates the entire packet before mutation.
 * ACK is published last. ST holds one request outstanding and waits for ACK.
 */
static inline void zy_fletcher_update(const unsigned char *p, unsigned n,
                                      unsigned *first, unsigned *second) {
    unsigned a=*first,b=*second;
    while(n) {
        unsigned count=n>20 ? 20 : n;
        n-=count;
        do { a+=*p++; b+=a; } while(--count);
        a=(a&255)+(a>>8); if(a>=255) a-=255;
        b=(b&255)+(b>>8);
        b=(b&255)+(b>>8); if(b>=255) b-=255;
    }
    *first=a; *second=b;
}
static inline unsigned zy_fletcher(const unsigned char *p, unsigned n) {
    unsigned a=0,b=0;
    zy_fletcher_update(p,n,&a,&b);
    return (b<<8)|a;
}
#endif
