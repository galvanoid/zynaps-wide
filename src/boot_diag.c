/* Diagnostic-only: export a fault PC through existing GPU counters, without
 * filesystem calls, new firmware, or depending on the TOS exception display. */
#include <stdint.h>
#include "protocol.h"
extern int zy_sc_rpc(unsigned,const uint8_t *,unsigned);
extern uint8_t _start[];
static uint8_t blank[ZY_MAX_PAYLOAD],args[3];
void zy_boot_diag_step(void){(void)zy_sc_rpc(ZY_LOAD,blank,sizeof blank);}
void zy_boot_diag_begin(void){(void)zy_sc_rpc(ZY_RESET,0,0);zy_boot_diag_step();}
void zy_boot_diag_fault(unsigned kind,uint32_t pc){
    uint32_t base=(uint32_t)(uintptr_t)_start;
    unsigned relative=pc>=base && pc<base+0x14000u;
    if(relative)pc-=base;
    for(unsigned i=0;i<((pc>>16)&255);i++)(void)zy_sc_rpc(127,0,0);
    for(unsigned i=0;i<((pc>>8)&255);i++)(void)zy_sc_rpc(ZY_RESET,0,0);
    for(unsigned i=0;i<(pc&255);i++)(void)zy_sc_rpc(ZY_DRAW,args,2);
    for(unsigned i=0;i<kind*10+relative;i++)(void)zy_sc_rpc(ZY_EMIT,args,3);
    for(;;)__asm__ volatile("nop");
}
