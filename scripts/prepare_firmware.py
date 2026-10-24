"""Integrate the GPU-demo transport with DevOps in a private build tree."""
from pathlib import Path
import shutil
import sys
import os

root = Path(__file__).resolve().parents[1]
repo = root.parent
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = Path(os.environ.get('ZY_DEVOPS_ROOT',root/'build/deps/md-devops'))
if not source.is_dir():source=repo/'vendor/md-devops'
for folder in ('rp/src', '.github/scripts'):
    shutil.copytree(source / folder, out / folder, dirs_exist_ok=True)
for folder in ('pico-sdk', 'pico-extras', 'fatfs-sdk'):
    # Reuse the populated SDK dependencies without modifying them.
    dep = Path(os.environ['ZY_DEPENDENCIES']) / folder if os.environ.get('ZY_DEPENDENCIES') else source / folder
    if not (dep / ('pico_sdk_init.cmake' if folder == 'pico-sdk' else
                   'external/pico_extras_import.cmake' if folder == 'pico-extras' else
                   'src/CMakeLists.txt')).exists():
        dep = repo / 'firmware/stnext-sc' / folder
    link = out / folder
    if not link.exists():
        link.symlink_to(dep, target_is_directory=True)
src = out / 'rp/src'
# PIO presents byte pairs in big-endian order. An odd final byte also needs
# swapping into the high lane; rounding up stays within the 4096-byte window.
gemdrive=src/'gemdrive.c'
text=gemdrive.read_text()
old='CHANGE_ENDIANESS_BLOCK16(dst, bytesRead & ~1u);'
assert text.count(old)==1
text=text.replace(old,'CHANGE_ENDIANESS_BLOCK16(dst, (bytesRead + 1u) & ~1u);')
# The actual target sends D3=handle, D4=offset, D5=mode. The upstream
# RP parser reversed its first two fields, so seeking to zero used handle 0.
old='''static void __not_in_flash_func(handleFseekCall)(uint16_t *payload) {
  uint32_t offset = TPROTO_GET_PAYLOAD_PARAM32(payload);
  TPROTO_NEXT32_PAYLOAD_PTR(payload);
  uint16_t handle = (uint16_t)TPROTO_GET_PAYLOAD_PARAM32(payload);'''
new='''static void __not_in_flash_func(handleFseekCall)(uint16_t *payload) {
  uint16_t handle = (uint16_t)TPROTO_GET_PAYLOAD_PARAM32(payload);
  TPROTO_NEXT32_PAYLOAD_PTR(payload);
  uint32_t offset = TPROTO_GET_PAYLOAD_PARAM32(payload);'''
assert text.count(old)==1
gemdrive.write_text(text.replace(old,new))
target_header = root / 'build/target/target_firmware.h'
if not target_header.is_file():
    raise RuntimeError('Build the cartridge target with build_target.sh first')
shutil.copy2(target_header, src / 'include/target_firmware.h')
for name in ('scroll_cache.h', 'scroll_cache.c', 'protocol.h', 'transport_mux.h', 'server.h', 'server.c', 'tile_decode.h', 'tile_decode.c'):
    shutil.copy2(root / 'src' / name, src / name)
full_gpu=os.environ.get('ZY_FULL_GPU')!='0'
if full_gpu:
    for name in ('sprite_atlas.h','sprite_compositor.h','sprite_compositor.c','resource_cache.h','resource_cache.c','cache_format.h','cache_validate.h','cache_validate.c','original_prg.h','original_prg.c','required_files.c','app_store.h','app_store.c'):
        shutil.copy2(root/'src'/name,src/name)

import subprocess
subprocess.run([sys.executable,str(root/'scripts/pack-program.py'),str(root/'dist/ZYWIDE.PRG'),str(src)],check=True)
cmake = (src / 'CMakeLists.txt').read_text()
cmake = cmake.replace('        emul.c\n', '        emul.c\n        scroll_cache.c\n        server.c\n        tile_decode.c\n        resource_cache.c\n        cache_validate.c\n        original_prg.c\n        required_files.c\n        app_store.c\n        program_bundle.c\n')
if full_gpu:
    cmake=cmake.replace('        server.c\n','        server.c\n        sprite_compositor.c\n')
    cmake+='\ntarget_compile_definitions(rp PRIVATE ZY_FULL_GPU=1)\n'
# Network/menu/setup code is outside the rendering hot path. Keep the GPU,
# transport and compositor at Release -O3, reserve flash through these files.
cmake+='\nset_source_files_properties(http_server.c emul.c app_store.c cache_validate.c display.c display_term.c term.c gemdrive.c runner.c sdcard.c network.c settings/settings.c PROPERTIES COMPILE_OPTIONS "-Os")\n'
(src / 'CMakeLists.txt').write_text(cmake)
# One owner drains the GPU demo's ROM3 DMA transport. Route our reserved
# addresses before the DevOps parser; GEMDOS/Runner packets remain unchanged.
handler = (src / 'chandler.c').read_text()
handler = handler.replace('#include "emul.h"', '#include "emul.h"\n#include "server.h"\n#include "transport_mux.h"\nzy_server zynaps_server;')
handler = handler.replace('  chandler_clear_pending_protocol();\n}',
    '  chandler_clear_pending_protocol();\n  zy_server_init(&zynaps_server, (uint16_t *)&__rom_in_ram_start__);\n}', 1)
handler = handler.replace('    uint16_t sample) {', '''    uint16_t sample) {
  _Static_assert(ZY_DEVOPS_HEADER_SAMPLE == (PROTOCOL_HEADER ^ CHANDLER_ADDRESS_HIGH_BIT),
                 "DevOps transport header changed");
  if (!zynaps_server.receiving && sample == ZY_BUS_START &&
      tprotocol_nextTPstep != HEADER_DETECTION &&
      timer_hw->timerawl - tprotocol_last_header_found > PROTOCOL_READ_RESTART_MICROSECONDS)
    tprotocol_nextTPstep = HEADER_DETECTION;
  unsigned route = zy_transport_route(sample, zynaps_server.receiving,
                                     tprotocol_nextTPstep != HEADER_DETECTION);
  if (route == ZY_ROUTE_DEVOPS_RESTART) {
    zynaps_server.receiving = 0;
    tprotocol_nextTPstep = HEADER_DETECTION;
  }
  if (route == ZY_ROUTE_GPU) {
    zy_server_sample(&zynaps_server, sample);
    return;
  }''', 1)
assert handler.count('unsigned route = zy_transport_route') == 1
(src / 'chandler.c').write_text(handler)
# Keep the measured 16 KB stack. The compact cache uses 28 KB of the
# original 51 KB heap budget; retain a 20 KB minimum (upstream measured
# at most 15 KB used by SD/Wi-Fi/HTTP). Link fails if that floor is crossed.
linker = (src / 'memmap_rp.ld').read_text().replace('__MinHeapSize = 40K;', '__MinHeapSize = 20K;')
linker=linker.replace('LENGTH = 1024k','LENGTH = 584k')
(src / 'memmap_rp.ld').write_text(linker)
# Zynaps does not produce a continuous debug stream. A 4 KB debug ring
# retains diagnostics while leaving >20 KB of heap for the network stack.
debug_header = src / 'include/debugcap.h'
debug_header.write_text(debug_header.read_text().replace('#define DEBUGCAP_RING_BYTES 8192', '#define DEBUGCAP_RING_BYTES 4096'))
# Do not sleep for 10 ms between graphics requests. Network and watchdog
# still run; requests after RESET use the busy polling path for low latency.
emul = (src / 'emul.c').read_text().replace('#include "emul.h"', '#include "emul.h"\n#include "server.h"\nextern zy_server zynaps_server;')
emul = emul.replace('cyw43_arch_wait_for_work_until(make_timeout_time_ms(SLEEP_LOOP_MS));',
    'if (!zynaps_server.resets) cyw43_arch_wait_for_work_until(make_timeout_time_ms(SLEEP_LOOP_MS));')
# Consumer boot enters GEMDRIVE/GEM after a short cancellable countdown.
# Runner remains available through U for development and recovery.
emul=emul.replace('#define BOOT_COUNTDOWN_SECONDS 20','#define BOOT_COUNTDOWN_SECONDS 3')
old='          countdownLaunching = true;\n          cmdRunner(NULL);\n          countdownLaunching = false;'
assert emul.count(old)==1
emul=emul.replace(old,'          countdownLaunching = true;\n          cmdGemdrive(NULL);\n          countdownLaunching = false;')
emul=emul.replace('#include "server.h"','#include "server.h"\n#include "app_store.h"')
emul=emul.replace('  // A failing card can take a few seconds to give up.','  zy_store_init();\n\n  // A failing card can take a few seconds to give up.')
emul=emul.replace('    sdcard_pollRemount();','    sdcard_pollRemount();\n    zy_store_poll();')
# This is an update to the existing Wide app. Migrate only its own directory
# settings; the app UUID and global Wi-Fi/STNext settings are unchanged.
marker='  int sdcardErr = sdcard_initFilesystem(&fsys, folderName);'
assert emul.count(marker)==1
emul=emul.replace(marker,'  SettingsConfigEntry *zyFolder=settings_find_entry(aconfig_getContext(), ACONFIG_PARAM_GEMDRIVE_FOLDER);\n  if(!folder || strcmp(folder->value,"/ZYNAPS") || !zyFolder || strcmp(zyFolder->value,"/ZYNAPS")){\n    settings_put_string(aconfig_getContext(), ACONFIG_PARAM_FOLDER, "/ZYNAPS");\n    settings_put_string(aconfig_getContext(), ACONFIG_PARAM_GEMDRIVE_FOLDER, "/ZYNAPS");\n    (void)saveAppSettings();\n  }\n  folderName="/ZYNAPS";\n'+marker)
(src / 'emul.c').write_text(emul)
http = (src / 'http_server.c').read_text()
http = http.replace('static void handle_ping(http_conn_t *c) {', '''#include "server.h"
extern zy_server zynaps_server;
static void handle_zynaps(http_conn_t *c) {
  char body[200];
  int n=snprintf(body,sizeof(body),
    "{\\"ok\\":true,\\"resets\\":%u,\\"loads\\":%u,\\"emits\\":%u,\\"draws\\":%u,\\"errors\\":%u}\\n",
    zynaps_server.resets,zynaps_server.loads,zynaps_server.emits,
    zynaps_server.draws,zynaps_server.errors);
  write_response(c,200,"OK","application/json",body,(size_t)n);
}
static void handle_ping(http_conn_t *c) {''')
http = http.replace('{"/api/v1/ping", M_GET | M_HEAD, handle_ping},',
    '{"/api/v1/ping", M_GET | M_HEAD, handle_ping},\n    {"/api/v1/zynaps", M_GET | M_HEAD, handle_zynaps},')
http=http.replace('char body[200];','char body[384];')
http=http.replace(r'\"errors\":%u}',
    r'\"errors\":%u,\"sparse_draws\":%u,\"sparse_rows\":%u,\"sparse_words\":%u}')
http=http.replace('zynaps_server.draws,zynaps_server.errors);',
    'zynaps_server.draws,zynaps_server.errors,zynaps_server.sparse_draws,\n'
    '    zynaps_server.sparse_rows,zynaps_server.sparse_words);')
http=http.replace(r'\"sparse_words\":%u}',
    r'\"sparse_words\":%u,\"composed_frames\":%u,\"composed_sprites\":%u}')
http=http.replace('zynaps_server.sparse_rows,zynaps_server.sparse_words);',
    'zynaps_server.sparse_rows,zynaps_server.sparse_words,\n'
    '    zynaps_server.composed_frames,zynaps_server.composed_sprites);')
# Keep file transfer and Runner launch; omit advanced diagnostic endpoints
# to reserve flash for the locally generated cache and clean PRG installer.
import re
def stub(text,name):
    m=re.search(r'static void '+name+r'\([^;{}]*\)\s*\{',text)
    assert m,name
    end=m.end();depth=1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[:m.end()]+'\n  write_error(c,404,"Not Found","not_found","Unavailable in this app");\n'+text[end-1:]
for name in ('handle_runner_load','handle_runner_unload','handle_runner_res',
             'handle_runner_adv_jump','handle_runner_adv_meminfo','handle_runner_adv_status',
             'handle_runner_meminfo','handle_runner_reset','handle_debug_status',
             'handle_debug_log','handle_debug_test','handle_system_health'):
    http=stub(http,name)
(src / 'http_server.c').write_text(http)
aconfig = src / 'aconfig.c'
aconfig.write_text(aconfig.read_text().replace('"/devops"', '"/ZYNAPS"'))
print('Prepared private firmware tree:', out)
