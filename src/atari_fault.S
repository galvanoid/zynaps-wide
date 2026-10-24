/* Emergency startup diagnostic: save the 68000 fault frame before Pterm.
 * Original vectors are restored before issuing any GEMDOS call. */
    .text
    .globl zy_fault_install
    .globl zy_fault_uninstall
zy_fault_uninstall:
    move.l zy_old_bus,8
    move.l zy_old_address,12
    move.l zy_old_illegal,16
    rts
    .globl zy_trace_open
    .globl zy_trace_read
zy_trace_read:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    move.l %d0,zy_read_trace
    move.l 68(%sp),zy_read_trace+4
    move.l 72(%sp),zy_read_trace+8
    move.l 76(%sp),zy_read_trace+12
    movea.l 76(%sp),%a0
    lea zy_read_trace+16,%a1
    moveq #15,%d0
9:  move.b (%a0)+,(%a1)+
    dbra %d0,9b
    move.b 0xfbff52,%d0
    lea zy_read_trace,%a2
    moveq #31,%d3
    bra zy_trace_bytes
zy_trace_open:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    move.l %d0,zy_open_trace
    movea.l 68(%sp),%a0
    lea zy_open_trace+4,%a1
    moveq #15,%d0
7:  move.b (%a0)+,(%a1)+
    dbra %d0,7b
    move.b 0xfbff4f,%d0
    lea zy_open_trace,%a2
    moveq #19,%d3
zy_trace_bytes:
8:  moveq #0,%d4
    move.b (%a2)+,%d4
    move.l %d4,%d0
    lsr.w #4,%d0
    bsr zy_debug_nibble
    move.l %d4,%d0
    and.w #15,%d0
    bsr zy_debug_nibble
    dbra %d3,8b
    move.b 0xfbff0a,%d0
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rts
zy_fault_install:
    move.l 8,zy_old_bus
    move.l 12,zy_old_address
    move.l 16,zy_old_illegal
    move.l #zy_bus_fault,8
    move.l #zy_address_fault,12
    move.l #zy_illegal_fault,16
    move.b 0xfbff49,%d0
    rts
zy_bus_fault:
    move.w #2,zy_fault_kind
    bra zy_fault
zy_address_fault:
    move.w #3,zy_fault_kind
    bra zy_fault
zy_illegal_fault:
    move.w #4,zy_fault_kind
zy_fault:
    movem.l %d0-%d7/%a0-%a6,zy_fault_regs
    move.l %sp,zy_fault_sp
    movea.l %sp,%a0
    lea zy_fault_frame,%a1
    moveq #7,%d0
1:  move.l (%a0)+,(%a1)+
    dbra %d0,1b
    lea zy_fault_stack_top,%sp
    move.l zy_old_bus,8
    move.l zy_old_address,12
    move.l zy_old_illegal,16
    move.l #_start,zy_fault_base
    movea.l #0x2ac0,%a0
    lea zy_fault_code,%a1
    moveq #31,%d0
6:  move.l (%a0)+,(%a1)+
    dbra %d0,6b
    /* Cartridge debug transport needs no working TOS stack or GEMDOS. */
    lea zy_fault_report,%a2
    move.w #231,%d3
3:  moveq #0,%d4
    move.b (%a2)+,%d4
    move.l %d4,%d0
    lsr.w #4,%d0
    bsr zy_debug_nibble
    move.l %d4,%d0
    and.w #15,%d0
    bsr zy_debug_nibble
    dbra %d3,3b
    move.b 0xfbff0a,%d0
    clr.w -(%sp)
    pea zy_fault_name
    move.w #0x3c,-(%sp)
    trap #1
    addq.l #8,%sp
    tst.l %d0
    bmi 2f
    move.w %d0,zy_fault_handle
    pea zy_fault_report
    move.l #232,-(%sp)
    move.w %d0,-(%sp)
    move.w #0x40,-(%sp)
    trap #1
    lea 12(%sp),%sp
    move.w zy_fault_handle,-(%sp)
    move.w #0x3e,-(%sp)
    trap #1
    addq.l #4,%sp
2:  move.w #-3,-(%sp)
    move.w #0x4c,-(%sp)
    trap #1
    bra 2b
zy_debug_nibble:
    add.w #48,%d0
    cmp.w #57,%d0
    bls 4f
    addq.w #7,%d0
4:  movea.l #0xfbff00,%a0
    move.b (%a0,%d0.w),%d1
    moveq #100,%d1
5:  dbra %d1,5b
    rts
zy_fault_name: .asciz "ZYFAULT.BIN"
    .bss
    .balign 4
zy_old_bus: .space 4
zy_open_trace: .space 20
zy_read_trace: .space 32
zy_old_address: .space 4
zy_old_illegal: .space 4
zy_fault_handle: .space 2
    .balign 4
zy_fault_report:
zy_fault_kind: .space 4
zy_fault_base: .space 4
zy_fault_regs: .space 60
zy_fault_sp: .space 4
zy_fault_frame: .space 32
zy_fault_code: .space 128
zy_fault_stack: .space 2048
zy_fault_stack_top:
