    .text
    .globl zy_boot_diag_install
zy_boot_diag_install:
    move.l 8,zy_boot_diag_old
    move.l 12,zy_boot_diag_old+4
    move.l 16,zy_boot_diag_old+8
    move.l #zy_boot_diag_bus,8
    move.l #zy_boot_diag_address,12
    move.l #zy_boot_diag_illegal,16
    jmp zy_boot_diag_begin
    .globl zy_boot_diag_uninstall
zy_boot_diag_uninstall:
    move.l zy_boot_diag_old,8
    move.l zy_boot_diag_old+4,12
    move.l zy_boot_diag_old+8,16
    rts
zy_boot_diag_bus:
    moveq #2,%d0
    bra zy_boot_diag_capture
zy_boot_diag_address:
    moveq #3,%d0
    bra zy_boot_diag_capture
zy_boot_diag_illegal:
    moveq #4,%d0
zy_boot_diag_capture:
    move.w #0x2700,%sr
    movea.l %sp,%a0
    lea zy_boot_diag_stack_top,%sp
    move.l 2(%a0),-(%sp)
    move.l %d0,-(%sp)
    jsr zy_boot_diag_fault
1:  bra 1b
    .bss
    .balign 4
zy_boot_diag_old:
    .space 12
    .space 4096
zy_boot_diag_stack_top:
    .section .note.GNU-stack,"",@progbits
