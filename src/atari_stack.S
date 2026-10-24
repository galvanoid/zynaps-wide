/* Runner may Pexec in supervisor mode on TOS's small system stack.
 * Reserve a game-owned stack after all other BSS objects. */
    .bss
    .balign 4
    .globl zy_private_stack_bottom
zy_private_stack_bottom:
    .space 16384
    .globl zy_private_stack_top
zy_private_stack_top:
