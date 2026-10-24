/* Packed 152-byte cart rows -> 160-byte Atari rows; preserve the right edge. */
    .text
    .globl zy_sc_copy_scene
zy_sc_copy_scene:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%a0
    move.l 52(%sp),%a1
    /* One image already composed on RP: no row decoder or sprite passes. */
    .rept 640
    movem.l (%a0)+,%d0-%d7/%a2-%a5
    movem.l %d0-%d7/%a2-%a5,(%a1)
    lea 48(%a1),%a1
    .endr
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

    /* XBIOS Blitmode(-1), bit 1: hardware Blitter present.  Called once. */
    .globl zy_sc_has_blitter
zy_sc_has_blitter:
    move.w #-1,-(%sp)
    move.w #64,-(%sp)
    trap #14
    addq.l #4,%sp
    and.l #2,%d0
    rts

    /* 320x192, 30,720 bytes.  The cart holds 640 blocks of 48 bytes in
     * reverse block order.  Shared mode lets the 68000 resolve collisions
     * while the Blitter consumes the finished Sidecart scene. */
    .globl zy_sc_copy_scene_blitter
zy_sc_copy_scene_blitter:
    movem.l %d2-%d4/%a2-%a3,-(%sp)
    move.l 24(%sp),%a2
    move.l 28(%sp),%a3
    lea 30672(%a3),%a3
    lea 0xffff8a20,%a0
    move.w #2,0(%a0)
    move.w #2,2(%a0)
    move.l %a2,4(%a0)
    moveq #-1,%d2
    move.w %d2,8(%a0)
    move.w %d2,10(%a0)
    move.w %d2,12(%a0)
    move.w #2,14(%a0)
    move.w #-94,16(%a0)
    move.l %a3,18(%a0)
    move.w #24,22(%a0)
    move.w #0x0203,26(%a0)
    clr.b 29(%a0)
    move.w #79,%d4
.Lzy_blit_sync_piece:
    move.w #8,24(%a0)
    move.b #0xc0,28(%a0)
    nop
    dbra %d4,.Lzy_blit_sync_piece
    movem.l (%sp)+,%d2-%d4/%a2-%a3
    rts

    .globl zy_sc_copy_scene_blitter_start
zy_sc_copy_scene_blitter_start:
    move.l 4(%sp),%a1
    move.l 8(%sp),%a2
    lea 30672(%a2),%a2
    lea 0xffff8a20,%a0
    move.w #2,0(%a0)
    move.w #2,2(%a0)
    move.l %a1,4(%a0)
    move.w #-1,8(%a0)
    move.w #-1,10(%a0)
    move.w #-1,12(%a0)
    move.w #2,14(%a0)
    move.w #-94,16(%a0)
    move.l %a2,18(%a0)
    move.w #24,22(%a0)
    move.w #640,24(%a0)
    move.w #0x0203,26(%a0)
    clr.b 29(%a0)
    move.b #0x80,28(%a0)
    rts

    .globl zy_sc_copy_scene_blitter_wait
zy_sc_copy_scene_blitter_wait:
    lea 0xffff8a20,%a0
.Lzy_blit_wait:
    btst #7,28(%a0)
    bne .Lzy_blit_wait
    rts

    .globl zy_sc_blitter_busy
zy_sc_blitter_busy:
    moveq #0,%d0
    btst #7,0xffff8a3c
    sne %d0
    andi.l #1,%d0
    rts

    /* Copy only the rectangles touched by sprites from the final Sidecart
     * scene.  The native scroll has already rebuilt the complete background;
     * opaque rectangle copies are therefore sufficient and preserve ordered
     * overlaps because the source contains the final composed pixels. */
    .globl zy_sc_copy_rects_blitter
zy_sc_copy_rects_blitter:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%a4
    move.l 52(%sp),%a5
    move.l 56(%sp),%a2
    move.l 60(%sp),%d7
    beq .Lzy_rect_done
    lea 0xffff8a20,%a6
    moveq #-1,%d6
    move.w %d6,8(%a6)
    move.w %d6,10(%a6)
    move.w %d6,12(%a6)
    move.w #2,0(%a6)
    move.w #2,14(%a6)
    move.w #0x0203,26(%a6)
    clr.b 29(%a6)
.Lzy_rect_next:
    moveq #0,%d0
    move.b 4(%a2),%d0
    lsl.w #8,%d0
    move.b 5(%a2),%d0
    add.w %d0,%d0
    move.l %a4,%a0
    adda.w %d0,%a0
    move.l %a5,%a1
    adda.w %d0,%a1
    move.l %a0,4(%a6)
    move.l %a1,18(%a6)
    moveq #0,%d1
    move.b 7(%a2),%d1
    and.w #127,%d1
    lsl.w #2,%d1
    move.w %d1,22(%a6)
    subq.w #1,%d1
    add.w %d1,%d1
    move.w #160,%d2
    sub.w %d1,%d2
    move.w %d2,2(%a6)
    move.w %d2,16(%a6)
    moveq #0,%d3
    move.b 6(%a2),%d3
    move.w %d3,24(%a6)
    move.b #0xc0,28(%a6)
    lea 12(%a2),%a2
    subq.l #1,%d7
    bne .Lzy_rect_next
.Lzy_rect_done:
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

    .globl zy_sc_copy_window
zy_sc_copy_window:
    move.l 8(%sp),-(%sp)
    jsr zy_sc_clear_gap
    addq.l #4,%sp
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%a0
    move.l 52(%sp),%a1
    move.w #143,%d7
1:
    cmp.w #71,%d7
    bne 7f
    lea 7296(%a0),%a0
    lea 7680(%a1),%a1
7:
    movem.l (%a0)+,%d0-%d6/%a2-%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    movem.l (%a0)+,%d0-%d6/%a2-%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    movem.l (%a0)+,%d0-%d6/%a2-%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    move.l (%a0)+,(%a1)+
    move.l (%a0)+,(%a1)+
    addq.l #8,%a1
    dbra %d7,1b
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

/* Run stream: high descriptor byte is blank cells, low byte literal cells.
 * Each row totals nineteen 16-pixel cells. The jump tables below are unrolled
 * instructions, four bytes per cell, avoiding a branch for every pixel cell. */
    .globl zy_sc_copy_runs
zy_sc_copy_runs:
    move.l 8(%sp),-(%sp)
    jsr zy_sc_clear_gap
    addq.l #4,%sp
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%a0
    move.l 52(%sp),%a1
    moveq #0,%d0
    move.w #143,%d7
.Lruns_row:
    cmp.w #71,%d7
    bne .Lruns_start
    lea 7680(%a1),%a1
.Lruns_start:
    moveq #19,%d5
.Lruns_command:
    moveq #0,%d2
    move.w (%a0)+,%d2
    move.w %d2,%d3
    lsr.w #8,%d3
    move.w %d2,%d4
    and.w #255,%d4
    move.w %d3,%d2
    add.w %d4,%d2
    beq .Lruns_bad
    cmp.w %d5,%d2
    bhi .Lruns_bad
    sub.w %d2,%d5
    moveq #19,%d1
    sub.w %d3,%d1
    lsl.w #2,%d1
    jmp .Lruns_zero(%pc,%d1.w)
.Lruns_zero:
    .rept 19
    move.l %d0,(%a1)+
    move.l %d0,(%a1)+
    .endr
    /* Four cells at once: MOVEM amortizes cartridge read/store instruction
     * overhead. d5/d7 keep row state; d6 keeps the literal remainder. */
    move.w %d4,%d6
.Lruns_bulk:
    cmp.w #4,%d6
    blo .Lruns_tail
    movem.l (%a0)+,%d0-%d4/%a2-%a4
    movem.l %d0-%d4/%a2-%a4,(%a1)
    lea 32(%a1),%a1
    subq.w #4,%d6
    bra .Lruns_bulk
.Lruns_tail:
    moveq #0,%d0
    moveq #19,%d1
    sub.w %d6,%d1
    lsl.w #2,%d1
    jmp .Lruns_literal(%pc,%d1.w)
.Lruns_literal:
    .rept 19
    move.l (%a0)+,(%a1)+
    move.l (%a0)+,(%a1)+
    .endr
    tst.w %d5
    bne .Lruns_command
    addq.l #8,%a1
    dbra %d7,.Lruns_row
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    moveq #1,%d0
    rts
.Lruns_bad:
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    moveq #0,%d0
    rts

/* Packed 8-byte cartridge edge -> final column of each 160-byte ST row. */
/* Sparse source rows, produced only when the firmware advertises SP/~SP.
 * Blank rows perform RAM writes only; opaque rows use the MOVEM cart path. */
    .globl zy_sc_copy_sparse
zy_sc_copy_sparse:
    move.l 8(%sp),-(%sp)
    jsr zy_sc_clear_gap
    addq.l #4,%sp
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%a0
    move.l 52(%sp),%a1
    move.w #143,%d7
.Lsparse_row:
    cmp.w #71,%d7
    bne .Lsparse_tag
    lea 7680(%a1),%a1
.Lsparse_tag:
    tst.w (%a0)+
    beq .Lsparse_blank
    movem.l (%a0)+,%d0-%d6/%a2-%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    movem.l (%a0)+,%d0-%d6/%a2-%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    movem.l (%a0)+,%d0-%d6/%a2-%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    move.l (%a0)+,(%a1)+
    move.l (%a0)+,(%a1)+
    bra .Lsparse_next
.Lsparse_blank:
    moveq #0,%d0
    moveq #0,%d1
    moveq #0,%d2
    moveq #0,%d3
    moveq #0,%d4
    moveq #0,%d5
    moveq #0,%d6
    suba.l %a2,%a2
    suba.l %a3,%a3
    suba.l %a4,%a4
    suba.l %a5,%a5
    suba.l %a6,%a6
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    movem.l %d0-%d6/%a2-%a6,(%a1)
    lea 48(%a1),%a1
    move.l %d0,(%a1)+
    move.l %d0,(%a1)+
.Lsparse_next:
    addq.l #8,%a1
    dbra %d7,.Lsparse_row
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

/* Packed 8-byte cartridge edge -> final column of each 160-byte ST row. */
    .globl zy_sc_copy_edge
zy_sc_copy_edge:
    movem.l %d2-%d4,-(%sp)
    move.l 16(%sp),%a0
    move.l 20(%sp),%a1
    move.w #191,%d4
2:
    movem.w (%a0)+,%d0-%d3
    movem.w %d0-%d3,(%a1)
    lea 160(%a1),%a1
    dbra %d4,2b
    movem.l (%sp)+,%d2-%d4
    rts

/* Original 144-row workspace -> two 72-row strips, 48 blank rows apart. */
    .globl zy_sc_workspace_edge
zy_sc_workspace_edge:
    movem.l %d2/%d4/%d6,-(%sp)
    move.l 16(%sp),%a0
    move.l 20(%sp),%a1
    move.w #143,%d1
3:
    move.l (%a0)+,%d0
    move.l (%a0)+,%d2
    move.l (%a0)+,%d4
    move.l (%a0)+,%d6
    swap %d0
    swap %d2
    swap %d4
    swap %d6
    movem.w %d0/%d2/%d4/%d6,(%a1)
    lea 160(%a1),%a1
    cmp.w #72,%d1
    bne 4f
    lea 7680(%a1),%a1
4:
    dbra %d1,3b
    movem.l (%sp)+,%d2/%d4/%d6
    rts
    .globl zy_sc_clear_gap
    .globl zy_sc_clear_playfield
zy_sc_clear_playfield:
    movem.l %d2-%d7,-(%sp)
    move.l 28(%sp),%a0
    move.w #959,%d0
    bra 6f
zy_sc_clear_gap:
    movem.l %d2-%d7,-(%sp)
    move.l 28(%sp),%a0
    lea 11520(%a0),%a0
    move.w #239,%d0
6:
    moveq #0,%d1
    moveq #0,%d2
    moveq #0,%d3
    moveq #0,%d4
    moveq #0,%d5
    moveq #0,%d6
    moveq #0,%d7
    movea.l %d1,%a1
8:  movem.l %d1-%d7/%a1,(%a0)
    lea 32(%a0),%a0
    dbra %d0,8b
    movem.l (%sp)+,%d2-%d7
    rts
