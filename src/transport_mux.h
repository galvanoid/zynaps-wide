#ifndef ZY_TRANSPORT_MUX_H
#define ZY_TRANSPORT_MUX_H
#include "protocol.h"
#define ZY_DEVOPS_HEADER_SAMPLE 0x2bcdu /* 0xabcd XOR 0x8000 */
enum { ZY_ROUTE_DEVOPS, ZY_ROUTE_GPU, ZY_ROUTE_DEVOPS_RESTART };
static inline unsigned zy_transport_route(unsigned sample,unsigned gpu_receiving,unsigned devops_active) {
    /* DevOps transmits arbitrary 16-bit words. In particular 0xe200 maps to
     * GPU START: only an idle DevOps parser may hand ownership to the GPU. */
    if(gpu_receiving) return sample==ZY_DEVOPS_HEADER_SAMPLE ? ZY_ROUTE_DEVOPS_RESTART : ZY_ROUTE_GPU;
    return !devops_active && sample==ZY_BUS_START ? ZY_ROUTE_GPU : ZY_ROUTE_DEVOPS;
}
#endif
