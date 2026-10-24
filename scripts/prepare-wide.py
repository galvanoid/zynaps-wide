"""Patch a private reconstruction copy; original Zynaps and vendor stay intact."""
from pathlib import Path
import re
import shutil
import os
root=Path(__file__).resolve().parents[1]
upstream=Path(os.environ.get('ZY_UPSTREAM_ROOT',root/'build/deps/atari_reverse'))
if not upstream.is_dir():upstream=root.parent/'zynaps/upstream'
source=upstream/'projects/zynaps/recreate'
out=Path(os.environ.get('ZY_RECREATE_ROOT', root/'build/recreate'))
shutil.copytree(source,out,dirs_exist_ok=True)

def replace(file,old,new,count=1):
    path=out/file
    text=path.read_text()
    assert text.count(old)==count,(file,old,text.count(old))
    path.write_text(text.replace(old,new))

def body(file,name,replacement):
    path=out/file
    text=path.read_text()
    match=re.search(r'\b'+name+r'\([^;{}]*\)\s*\{',text)
    assert match,(file,name)
    start=match.end(); end=start; depth=1
    while depth:
        if text[end]=='{': depth+=1
        if text[end]=='}': depth-=1
        end+=1
    path.write_text(text[:start]+'\n'+replacement+'\n'+text[end-1:])

replace('include/video.h','#define PLAYFIELD_ROWS    144u','#define PLAYFIELD_ROWS    192u')
replace('include/video.h','#define PLAYFIELD_BYTES 0x5a00u','#define PLAYFIELD_BYTES 0x7800u')
# Background pages and map remain 18 tiles high. Presentation inserts six
# blank tile rows between the upper and lower nine rows, without stretching.
replace('src/scroll.c','row < PLAYFIELD_ROWS','row < 144u')
replace('include/player.h','#define SHIP_Y_MAX 0x9c','#define SHIP_Y_MAX 0xcc')
replace('include/init.h','#define SECTION_RESTART_SHIP_Y 0x64u','#define SECTION_RESTART_SHIP_Y 0x7cu')
replace('include/frame.h','#define SHOT_Y_MAX 0xb0','#define SHOT_Y_MAX 0xe0')
replace('src/entity.c','#define ENTITY_KEEP_Y_MAX 0xb0','#define ENTITY_KEEP_Y_MAX 0xe0')
replace('src/enemy.c','#define ACTOR_KEEP_Y_MAX  0xb0','#define ACTOR_KEEP_Y_MAX  0xe0')
replace('src/enemy.c','#define ACTOR_FLOOR_Y 0xa0','#define ACTOR_FLOOR_Y 0xd0')
replace('src/enemy.c','#define ACTOR_CENTRE_Y 0x60','#define ACTOR_CENTRE_Y 0x78')
replace('src/asm/sprite.S','.equ PLAYFIELD_ROWS,           144','.equ PLAYFIELD_ROWS,           192')
# Keep the original 144 source rows. Skip 48 destination rows halfway down;
# the variant wrapper clears that middle band before the native RAM blit.
path=out/'src/asm/scroll_blit.S'
text=path.read_text()
text,n=re.subn(r'(\.L\d\d_row:\n)',r'\1    cmp.w #71,%d0\n    bne 9f\n    lea 7680(%a6),%a6\n9:\n',text)
assert n==20,n
path.write_text(text)
# Coordinates in ground scripts refer to the original terrain. Preserve
# ceiling actors; move lower actors together with the lower terrain.
replace('src/enemy.c','    if (spawn_y == 0)',
        '    if ((uint16_t)spawn_y >= 104) spawn_y = set_low_word(spawn_y, (uint16_t)spawn_y + 48);\n    if (spawn_y == 0)')
replace('src/enemy.c','        wr16(image + slot + ENTITY_Y, y);',
        '        if (actor_type <= 2) y += 24;\n        else if (y >= 104) y += 48;\n        wr16(image + slot + ENTITY_Y, y);')
replace('src/enemy.c','wr16(image + record + ACTOR_SINE_BASE_Y, base_y);',
        'wr16(image + record + ACTOR_SINE_BASE_Y, base_y >= 104 ? base_y + 48 : base_y);')
# The compact panel never changes game state, including the weapon slot.
body('src/frame.c','frame_panel_repaint','    (void)image;')
for name in ('status_panel_build_master','status_panel_redraw_all'):
    body('src/hud.c',name,'    image[A_panel_redraw_mask]=0;')
# The per-life shelf also invokes the original icon routines directly. They
# must not stamp the old panel over the expanded playfield or the intro screen.
for name in ('hud_draw_weapon_icon','hud_draw_powerup_icon','hud_draw_logo_anim','draw_power_gauge'):
    args='    (void)image;'
    if name=='hud_draw_weapon_icon': args+='\n    (void)right_cell;'
    body('src/hud.c',name,args)
body('src/hud.c','draw_lives_icons','    image[A_panel_redraw_mask] &= (uint8_t)~(1u << PANEL_REDRAW_LIVES_BIT);')
replace('src/hud.c','static void install_frontend_palette(uint8_t *image) {',
        'extern uint8_t wide_playing;\nextern void wide_hud_draw(uint8_t *, uint32_t);\nstatic void install_frontend_palette(uint8_t *image) {\n    wide_playing=0;')
replace('src/hud.c','void player_intro_screen(uint8_t *image) {',
        'void player_intro_screen(uint8_t *image) {\n    wide_playing=0;')
replace('src/hud.c','    playfield_clear(image);','    screen_clear(image, buffer);')
replace('src/hud.c','    screen_flip_buffers(image);\n    install_frontend_palette(image);',
        '    wide_hud_draw(image, buffer);\n    screen_flip_buffers(image);\n    install_frontend_palette(image);')
replace('src/hud.c','void title_screen_draw(uint8_t *image) {',
        'void title_screen_draw(uint8_t *image) {\n    wide_playing=0;')
# During gameplay both interrupt paths must publish the SAME section palette.
# The old Timer B palette cycles belonged to the large panel and otherwise
# recolour terrain now occupying its scanlines. Keep title cycling intact.
replace('src/irq.c','void timer_b_raster_isr(uint8_t *image) {',
        'extern uint8_t wide_playing;\nvoid timer_b_raster_isr(uint8_t *image) {\n'
        '    if (wide_playing) {\n        shifter_upload_palette_longs(image, A_menu_palette);\n'
        '        mfp_ack_timer_b();\n        return;\n    }')
replace('src/frame.c','frame_exit frame_loop_once(uint8_t *image, uint32_t chance_index_register,\n                           uint32_t ground_spawn_y_register) {',
        'extern uint8_t wide_playing;\nframe_exit frame_loop_once(uint8_t *image, uint32_t chance_index_register,\n                           uint32_t ground_spawn_y_register) {\n    wide_playing=1;')
body('src/video.c','playfield_clear',
     '    extern void zy_sc_clear_playfield(void *);\n    zy_sc_clear_playfield(image + be32(image + A_screen_back));')
replace('src/frame.c','#include "frame.h"','#include "frame.h"\n#include "stage_profile.h"')
replace('src/frame.c','    wide_playing=1;','    wide_playing=1;\n    zy_stage_begin(0, image);')
replace('src/frame.c','    frame_spawn_and_move_stage(image, chance_index_register, ground_spawn_y_register);\n    ZY_FRAME(frame_draw_objects_and_collide)(image);',
        '    zy_stage_end(0);\n    zy_stage_begin(1, image);\n'
        '    frame_spawn_and_move_stage(image, chance_index_register, ground_spawn_y_register);\n'
        '    zy_stage_end(1);\n    zy_stage_begin(2, image);\n    ZY_FRAME(frame_draw_objects_and_collide)(image);')
replace('src/frame.c','    ZY_FRAME(frame_draw_objects_and_collide)(image);',
        '    ZY_FRAME(frame_draw_objects_and_collide)(image);\n    zy_stage_end(2);\n    zy_stage_begin(3, image);')
replace('src/frame.c','    return ZY_FRAME(frame_resolve_hits_and_game_state)(image, ENTITY_SLOTS);',
        '    frame_exit result=ZY_FRAME(frame_resolve_hits_and_game_state)(image, ENTITY_SLOTS);\n'
        '    zy_stage_end(3);\n    return result;')
replace('src/asm/frame_draw.S','    move.w  #COLLISION_MASK_LONGS - 1,%d0',
        '#ifdef ZY_PROFILE\n    movem.l %d0-%d1/%a0-%a1,-(%sp)\n'
        '    jsr zy_profile_sprites_done\n    movem.l (%sp)+,%d0-%d1/%a0-%a1\n#endif\n'
        '    move.w  #COLLISION_MASK_LONGS - 1,%d0')
# Only the gameplay flip draws the compact HUD; title and high scores retain
# their own presentation. Draw before waiting for the raster, not after it.
replace('src/frame.c','static void frame_end_and_flip(uint8_t *image) {',
        'extern void wide_hud_draw(uint8_t *, uint32_t);\nstatic void frame_end_and_flip(uint8_t *image) {\n    zy_gpu_present();\n    wide_hud_draw(image, be32(image + A_screen_back));')
replace('src/asm/frame.S','.L126ee:\n',
        '.L126ee:\n    movem.l %d0-%d1/%a0-%a1,-(%sp)\n#ifdef ZY_FULL_GPU\n    jsr zy_gpu_present\n#endif\n    move.l A_screen_back-FGB(%a5),-(%sp)\n'
        '    move.l %a6,-(%sp)\n    jsr wide_hud_draw\n'
        '    lea 8(%sp),%sp\n    movem.l (%sp)+,%d0-%d1/%a0-%a1\n.Lwide_raster_wait:\n')
replace('src/asm/frame.S','jbne    .L126ee                                 | 0126f6',
        'jbne    .Lwide_raster_wait                      | 0126f6')
# Front-end compose buffers are still 144 rows, independently of gameplay.
replace('src/video.c','zero_longs(image, A_backdrop_page0, PLAYFIELD_BYTES);',
        'zero_longs(image, A_backdrop_page0, 144u * SCREEN_ROW_BYTES);')
replace('src/scroll.c','be32(image + A_screen_back), PLAYFIELD_BYTES);',
        'be32(image + A_screen_back), 144u * SCREEN_ROW_BYTES);')
# Full-scene collection is inert without its capability; title remains native.
replace('src/frame.c','#include "stage_profile.h"',
        '#include "stage_profile.h"\n#include "full_gpu.h"')
replace('src/frame.c','    wide_playing=1;',
        '    wide_playing=1;\n    zy_gpu_begin(image);')
replace('src/frame.c','    for (index = 0; index < COLLISION_MASK_LONGS; index++)',
        '    zy_gpu_flush();\n    for (index = 0; index < COLLISION_MASK_LONGS; index++)')
replace('src/asm/frame_draw.S','#ifdef ZY_PROFILE\n    movem.l',
        '#ifdef ZY_FULL_GPU\n    movem.l %d0-%d1/%a0-%a1,-(%sp)\n'
        '    jsr zy_gpu_flush\n    movem.l (%sp)+,%d0-%d1/%a0-%a1\n#endif\n'
        '#ifdef ZY_PROFILE\n    movem.l')
replace('src/video.c','    extern void zy_sc_clear_playfield(void *);',
        '    extern void zy_sc_clear_playfield(void *);\n'
        '#ifdef ZY_FULL_GPU\n    extern int zy_gpu_clear(uint8_t *);\n'
        '    if(zy_gpu_clear(image))return;\n#endif')
print('Prepared 192-row private reconstruction:',out)
import runpy
runpy.run_path(str(root/'scripts/reapply-fixes.py'), init_globals={'recreate':out})
