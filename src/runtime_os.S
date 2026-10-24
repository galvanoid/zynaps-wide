    .text
    .globl zy_console
zy_console:
    movem.l %d2/%a2,-(%sp)
    move.l 12(%sp),-(%sp)
    move.w #9,-(%sp)
    trap #1
    addq.l #6,%sp
    movem.l (%sp)+,%d2/%a2
    rts
    .globl zy_console_wait
zy_console_wait:
    movem.l %d2/%a2,-(%sp)
    move.w #7,-(%sp)
    trap #1
    addq.l #2,%sp
    movem.l (%sp)+,%d2/%a2
    rts
    .globl zy_fseek
zy_fseek:
    movem.l %d2/%a2,-(%sp)
    move.l 12(%sp),%d1
    move.l 16(%sp),%d0
    move.l 20(%sp),%d2
    move.w %d2,-(%sp)
    move.w %d0,-(%sp)
    move.l %d1,-(%sp)
    move.w #0x42,-(%sp)
    trap #1
    lea 10(%sp),%sp
    movem.l (%sp)+,%d2/%a2
    rts
    .globl zy_fdelete
zy_fdelete:
    movem.l %d2/%a2,-(%sp)
    move.l 12(%sp),-(%sp)
    move.w #0x41,-(%sp)
    trap #1
    addq.l #6,%sp
    movem.l (%sp)+,%d2/%a2
    rts
    .globl zy_frename
zy_frename:
    movem.l %d2/%a2,-(%sp)
    move.l 16(%sp),-(%sp)
    move.l 16(%sp),-(%sp)
    clr.w -(%sp)
    move.w #0x56,-(%sp)
    trap #1
    lea 12(%sp),%sp
    movem.l (%sp)+,%d2/%a2
    rts
    .section .note.GNU-stack,"",@progbits
