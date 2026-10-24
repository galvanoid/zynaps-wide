#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/scripts/prepare-wide.py"
REC="${ZY_RECREATE_ROOT:-$ROOT/build/recreate}"
UPSTREAM="${ZY_UPSTREAM_ROOT:-$ROOT/build/deps/atari_reverse}"
[ -d "$UPSTREAM" ] || UPSTREAM="$ROOT/../zynaps/upstream"
KIT="$UPSTREAM/tools/recreate_kit"
BUILD="${ZY_BUILD_ROOT:-$ROOT/build}/atari"
DIST="${ZY_DIST_ROOT:-$ROOT/dist}"
CC="${ZY_CC:-m68k-elf-gcc}"
PREFIX="${CC%gcc}"
mkdir -p "$BUILD" "$DIST"
program_bytes=0x9f46
if [ -f "$ROOT/dist/ZYNAPS.IMG" ]; then
    program_bytes=$(wc -c < "$ROOT/dist/ZYNAPS.IMG")
    [ "$program_bytes" -eq 40774 ] || { echo 'Unexpected program image size'; exit 1; }
fi
defs=(-DZY_PHASE=1 -DZY_SMOKE_VBLS=0xffffffffu -DZY_GAME_FRAMES=0xffffffffu
      -DZY_FRAME_SAMPLES=0u -DPROGRAM_BYTES="$program_bytes" -DZY_LOAD_BASE=0x10000
      -DOS_NO_REFUSAL_TALLY)
if [ "${ZY_MODE:-play}" = game ]; then
    defs=(-DZY_PHASE=1 -DPROGRAM_BYTES="$program_bytes" -DZY_LOAD_BASE=0x10000 -DOS_NO_REFUSAL_TALLY -DZY_TARGET_HARNESS)
fi
if [ "${ZY_BOOT_DIAG:-0}" = 1 ]; then defs+=(-DZY_BOOT_DIAG); fi
if [ "${ZY_PROFILE:-0}" = 1 ]; then defs+=(-DZY_PROFILE); fi
if [ "${ZY_GPU_DISABLE:-0}" = 1 ]; then defs+=(-DZY_GPU_DISABLE); fi
if [ "${ZY_SPARSE_EXPERIMENTAL:-0}" = 1 ]; then defs+=(-DZY_SPARSE_EXPERIMENTAL); fi
if [ "${ZY_FULL_GPU:-1}" = 1 ]; then defs+=(-DZY_FULL_GPU); fi
if [ "${ZY_ASYNC_BLITTER:-0}" = 1 ]; then defs+=(-DZY_ASYNC_BLITTER); fi
while read -r seam; do defs+=("-D$seam"); done < <(grep -ohE '^#ifdef +ZY_ASM_[A-Z]+' "$REC"/include/*.h | awk '{print $2}' | sort -u)
flags=(-m68000 -O2 -ffunction-sections -fdata-sections -fno-store-merging -fno-tree-loop-distribute-patterns -fno-jump-tables -ffreestanding
       -fomit-frame-pointer -nostdlib -fno-pie -I"$REC/atari/shim_include"
       -I"$REC/include" -I"$REC/atari" -I"$KIT/include" -I"$ROOT/src" -Wall -Wextra)
objects=()
printf '/* No game assets in distribution builds. */\n' > "$BUILD/assets.c"
terrain=()
meter=()
boss2x=()
pacing=()
gpu_source="$ROOT/src/full_gpu.c"
if grep -q '^#define ZY_FIX_PACING ' "$REC/include/next_features.h"; then
    pacing=("$ROOT/src/frame_pacing.c")
    # Compare presentation alone against the physically accepted GPU client.
    gpu_source="$ROOT/src/full_gpu.c"
fi
if grep -q '^#define ZY_FIX_BOSS2X ' "$REC/include/next_features.h"; then
    boss2x=("$ROOT/src/boss2x.c" "$ROOT/src/scale2x.c")
fi
if grep -q '^#define ZY_FIX_MEASURE ' "$REC/include/next_features.h"; then
    meter=("$ROOT/src/frame_meter.c")
fi
if grep -q '^#define ZY_FIX_TERRAIN ' "$REC/include/next_features.h"; then
    terrain=("$ROOT/src/terrain.c")
fi
# The upstream AUTO entry assumes a user stack at the top of its TPA.
# Runner's supervisor Pexec can instead inherit TOS's tiny system stack.
# Patch only a private build copy; own the C stack before any C is called.
sed '/jsr     zynaps_main/i\    lea zy_private_stack_top,%sp\n    move.l %sp,zy_initial_sp' \
    "$REC/atari/zynaps_os.s" > "$BUILD/zynaps_os.s"
diagnostic=()
if [ "${ZY_DIAGNOSTIC:-0}" = 1 ]; then
diagnostic=("$ROOT/src/atari_fault.S")
sed -i '/jsr     zynaps_main/i\    jsr zy_fault_install' "$BUILD/zynaps_os.s"
sed -i '/^Fopen:/,/^Fclose:/ { /^    rts/i\    jsr zy_trace_open
}' "$BUILD/zynaps_os.s"
sed -i '/^zy_leave_supervisor:/a\    jsr zy_fault_uninstall' "$BUILD/zynaps_os.s"
sed -i '/^Fread:/,/^Fwrite:/ { /^    rts/i\    jsr zy_trace_read
}' "$BUILD/zynaps_os.s"
fi
# The upstream budget counts all BSS as non-stack payload. Our final BSS
# object is now the reserved stack itself, so measure from its lower bound.
sed 's/uint32_t loaded_top = basepage_long(BASEPAGE_P_BBASE) + basepage_long(BASEPAGE_P_BLEN);/extern uint8_t zy_private_stack_bottom[]; uint32_t loaded_top = (uint32_t)(uintptr_t)zy_private_stack_bottom;/' \
    "$REC/atari/zynaps_main.c" > "$BUILD/zynaps_main.c"
if [ "${ZY_DIAGNOSTIC:-0}" = 1 ]; then
    sed -i '/write_file(FILE_ANCHOR_BASE/i\    (void)*(volatile unsigned char *)0xfbff42;' "$BUILD/zynaps_main.c"
    sed -i '/write_file(FILE_ANCHOR_BASE/a\    (void)*(volatile unsigned char *)0xfbff62;' "$BUILD/zynaps_main.c"
    sed -i '/boot_load_title_assets(zy_image_base);/i\    (void)*(volatile unsigned char *)0xfbff54;' "$BUILD/zynaps_main.c"
    sed -i '/boot_load_title_assets(zy_image_base);/a\    (void)*(volatile unsigned char *)0xfbff74;' "$BUILD/zynaps_main.c"
    sed -i '/boot_load_gameplay_assets(zy_image_base);/i\    (void)*(volatile unsigned char *)0xfbff47;' "$BUILD/zynaps_main.c"
    sed -i '/boot_load_gameplay_assets(zy_image_base);/a\    (void)*(volatile unsigned char *)0xfbff67;' "$BUILD/zynaps_main.c"
    sed -i '/section_start_prefill(zy_image_base);/i\    (void)*(volatile unsigned char *)0xfbff50;' "$BUILD/zynaps_main.c"
fi
python3 "$ROOT/scripts/patch-preparation.py" "$REC"
python3 "$ROOT/scripts/patch-app-main.py" "$BUILD/zynaps_main.c"
if [ "${ZY_BOOT_DIAG:-0}" = 1 ]; then
    diagnostic+=("$ROOT/src/boot_diag.c" "$ROOT/src/boot_diag.S")
    sed -i '/jsr     zynaps_main/i\    jsr zy_boot_diag_install' "$BUILD/zynaps_os.s"
    sed -i '/^zy_leave_supervisor:/a\    jsr zy_boot_diag_uninstall' "$BUILD/zynaps_os.s"
fi
for file in "$BUILD"/zynaps_os.s "$BUILD"/zynaps_main.c "$REC"/atari/zynaps_backend.c \
            "$REC"/atari/zynaps_cheats.c "$REC"/src/*.c "$REC"/src/asm/*.S \
            "$ROOT/src/atari_sc.c" "$gpu_source" "$ROOT/src/sprite_commands_68k.S" "$ROOT/src/tile_decode.c" "$ROOT/src/wide_hud.c" "$BUILD/assets.c" "$ROOT/src/original_prg.c" "$ROOT/src/runtime_io.c" "$ROOT/src/cache_setup.c" "$ROOT/src/cache_writer.c" "$ROOT/src/cache_validate.c" "$ROOT/src/runtime_os.S" "$ROOT/src/required_files.c" "$ROOT/src/map_projection.c" "$ROOT/src/terrain_metadata.c" "$ROOT/src/atari_copy.S" "${terrain[@]}" "${meter[@]}" "${boss2x[@]}" "${pacing[@]}" "${diagnostic[@]}" "$ROOT/src/atari_stack.S"; do
    obj="$BUILD/$(basename "$file").o"
    extra=()
    if [ "$(basename "$file")" = video.c ]; then extra=(-funroll-loops); fi
    "$CC" "${flags[@]}" "${defs[@]}" "${extra[@]}" -c "$file" -o "$obj"
    objects+=("$obj")
done
# Exclude verification-only twins, as the upstream target does.
filtered=()
for obj in "${objects[@]}"; do
    case "$obj" in
        *.S.o)
            if "${PREFIX}nm" -g --defined-only "$obj" | grep -qE ' (frame_panel_scroll_and_ship_stage|frame_drone_and_fire_stage|frame_spawn_and_move_stage)_asm$'; then continue; fi ;;
    esac
    filtered+=("$obj")
done
objects=("${filtered[@]}")
wraps=(-Wl,--wrap=section_start_prefill)
if [ "${ZY_FULL_GPU:-1}" = 1 ]; then
    wraps+=(-Wl,--wrap=draw_sprite_masked_asm -Wl,--wrap=draw_sprite_masked_collide_asm)
fi
wraps+=(-Wl,--wrap=Fopen -Wl,--wrap=Fread -Wl,--wrap=Fclose)
for symbol in scroll_emit_tile_column scroll_emit_column_shift0 scroll_emit_column_shift2; do
    wraps+=("-Wl,--wrap=${symbol}_asm")
done
for phase in $(seq -w 0 19); do wraps+=("-Wl,--wrap=scroll_page_to_screen_p${phase}_asm"); done
"$CC" "${flags[@]}" -T "$REC/atari/tos.ld" -Wl,--emit-relocs,--gc-sections \
    -Wl,-Map,"$BUILD/ZYNAPS.map" "${wraps[@]}" "${objects[@]}" -lgcc -o "$BUILD/ZYNAPS.elf"
entry=$("${PREFIX}nm" "$BUILD/ZYNAPS.elf" | awk '$3=="_start" {print $1}')
[ "$entry" = 00000000 ] || { echo "GEMDOS entry must be at text offset zero, found $entry"; exit 1; }
"${PREFIX}objcopy" -O binary "$BUILD/ZYNAPS.elf" "$BUILD/ZYNAPS.bin"
# mkprg expects the ELF tool name; adapt in this build tree only.
sed "s/m68k-elf-/${PREFIX}/g" "$REC/atari/mkprg.py" > "$BUILD/mkprg.py"
python3 "$BUILD/mkprg.py" "$BUILD/ZYNAPS.elf" "$BUILD/ZYNAPS.bin" "$DIST/ZYNAPS.PRG"
python3 "$ROOT/scripts/pad-prg.py" "$DIST/ZYNAPS.PRG"
cp "$BUILD/ZYNAPS.elf" "$DIST/ZYNAPS.elf"
echo 'Built asset-free ZYNAPS.PRG; original files required at runtime.'
