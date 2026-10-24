/* Keep the original maximum 25 simulation steps/sec, without forcing a late
 * frame to wait for the next even VBL. The rendered back buffer is selected
 * BEFORE a VBL, then we wait for that VBL before reusing the old front buffer.
 * No IRQ, sound, sprite, HUD or collision algorithm changes. */
#include "frame_pacing.h"
#include "machine.h"
#include "frame.h"
#include "video.h"
#include "input.h"
#include "irq.h"
extern volatile uint32_t zy_vbl_ticks;
extern uint8_t wide_playing;
static uint32_t last_present;
static unsigned initialized;
static uint32_t clock_vbl(void){return zy_vbl_ticks;}
static void wait_tick(void){
#ifdef ZY_PACING_TEST
    extern void zy_pace_test_wait(void);
    zy_pace_test_wait();
#else
    __asm__ volatile("nop":::"memory");
#endif
}
void zy_pace_begin(void) {
    if(!initialized || !wide_playing){last_present=clock_vbl();initialized=1;}
}
void zy_pace_present(uint8_t *image) {
    /* At least one VBL since the last completed presentation, followed by
     * one more AFTER selecting the screen: never >25 game updates/sec. */
    while((uint32_t)(clock_vbl()-last_present)<1u)wait_tick();
    screen_flip_buffers(image);
    uint32_t selected=clock_vbl();
    ikbd_send_cmd(IKBD_CMD_INTERROGATE_JOYSTICK);
    image[A_vbl_wait_flag]=FRAME_VBL_WAIT_ARMED;
    while(clock_vbl()==selected)wait_tick();
    last_present=clock_vbl();
    image[A_vbl_wait_flag]=FRAME_VBL_WAIT_DONE;
}
