/* Fast 68000 implementation of zy_sprite_command.
 *
 * The C reference remains the executable specification used by host and
 * differential tests.  This routine is deliberately leaf-only: enemy-heavy
 * frames must not pay for libgcc arithmetic or duplicated packet writers.
 * The twelve-byte command can start at an odd address, so every output is a
 * byte store (MOVE.W/L would bus-error on a plain 68000).
 */
    .text
    .even
    .globl zy_sprite_command
zy_sprite_command:
    movem.l %d2-%d7/%a2-%a4,-(%sp)
    move.l 40(%sp),%a4             /* image */
    move.l 44(%sp),%d0             /* entity offset */
    lea (%a4,%d0.l),%a1
    move.l 52(%sp),%a3             /* collide */
    move.l 56(%sp),%a2             /* odd-safe output */

    move.w 0(%a1),%d2              /* signed x, forced even */
    ext.l %d2
    andi.w #0xfffe,%d2
    move.w 4(%a1),%d3              /* signed y */
    ext.l %d3
    moveq #0,%d4
    move.w 8(%a1),%d4              /* height */
    cmpa.l #0,%a3
    beq 1f
    andi.l #0x7fff,%d4
1:
    tst.l %d4
    beq .Lbad
    cmpi.l #40,%d4
    bhi .Lbad
    cmpi.l #224,%d3
    bge .Lclip
    move.l %d3,%d0
    add.l %d4,%d0
    cmpi.l #32,%d0
    ble .Lclip
    cmpa.l #0,%a3
    bne 2f
    tst.l %d2
    bmi .Lclip
    cmpi.l #320,%d2
    bge .Lclip
2:
    move.l 10(%a1),%d5             /* sprite source */
    move.l %d2,%d0
    andi.l #15,%d0                 /* shift phase */
    cmpa.l #0,%a3
    beq 3f
    move.l %d4,%d1
    mulu.w #5,%d1
    bra 4f
3:
    move.w 50(%sp),%d1             /* low word of unsigned step */
4:
    mulu.w %d0,%d1
    add.l %d1,%d5

    moveq #0,%d6                   /* destination byte offset */
    cmpi.l #32,%d3
    bge 6f
    moveq #32,%d0
    sub.l %d3,%d0                  /* hidden rows */
    move.l %d0,%d1
    add.l %d1,%d1                  /* 2h */
    add.l %d1,%d1                  /* 4h */
    add.l %d0,%d1                  /* 5h */
    add.l %d1,%d1                  /* 10h */
    add.l %d1,%d5
    sub.l %d0,%d4
    bra 8f
6:
    move.l %d3,%d0
    add.l %d4,%d0
    cmpi.l #224,%d0
    ble 7f
    move.l #224,%d4
    sub.l %d3,%d4
7:
    move.l %d3,%d6
    sub.l #32,%d6
    move.l %d6,%d0
    lsl.l #2,%d6                   /* 4y */
    add.l %d0,%d6                  /* 5y */
    lsl.l #4,%d6                   /* 80y */
8:
    cmpa.l #0,%a3
    beq .Lplain

    move.l %d2,%d0
    andi.l #15,%d0
    add.l %d0,%d0
    move.l #0x1821e,%a0
    adda.l %a4,%a0
    move.l (%a0,%d0.l),%d7        /* collision keep mask */
    moveq #2,%d1                   /* cells */
    cmpi.l #64,%d2
    bge .Lcollision_middle
    cmpi.l #48,%d2
    ble .Lclip
    not.l %d7
    moveq #1,%d1
    bra .Lcollapse
.Lcollision_middle:
    cmpi.l #368,%d2
    ble .Lcollision_position
    cmpi.l #384,%d2
    bge .Lclip
    addi.l #76,%d6
    moveq #1,%d1
    bra .Lcollapse
.Lcollision_position:
    move.l %d2,%d0
    subi.l #64,%d0
    andi.l #0xfff0,%d0
    lsr.l #2,%d0
    add.l %d0,%d6
.Lcollapse:
    cmpi.l #-1,%d7
    bne .Lemit
    moveq #1,%d1
    bra .Lemit

.Lplain:
    moveq #-1,%d7
    moveq #1,%d1
    move.l %d2,%d0
    andi.l #0xfff0,%d0
    lsr.l #2,%d0
    add.l %d0,%d6

.Lemit:
    /* Source, big endian. Four rotates avoid scratch-register pressure. */
    rol.l #8,%d5
    move.b %d5,(%a2)+
    rol.l #8,%d5
    move.b %d5,(%a2)+
    rol.l #8,%d5
    move.b %d5,(%a2)+
    rol.l #8,%d5
    move.b %d5,(%a2)+
    move.w %d6,%d0
    ror.w #8,%d0
    move.b %d0,(%a2)+
    rol.w #8,%d0
    move.b %d0,(%a2)+
    move.b %d4,(%a2)+
    cmpa.l #0,%a3
    beq 9f
    ori.b #0x80,%d1
9:
    move.b %d1,(%a2)+
    rol.l #8,%d7
    move.b %d7,(%a2)+
    rol.l #8,%d7
    move.b %d7,(%a2)+
    rol.l #8,%d7
    move.b %d7,(%a2)+
    rol.l #8,%d7
    move.b %d7,(%a2)+
    moveq #1,%d0
    bra .Lreturn
.Lclip:
    moveq #0,%d0
    bra .Lreturn
.Lbad:
    moveq #-1,%d0
.Lreturn:
    movem.l (%sp)+,%d2-%d7/%a2-%a4
    rts

    .section .note.GNU-stack,"",@progbits
