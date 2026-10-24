"""Apply distribution-only startup checks to a private generated source file."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
old='void zynaps_main(void) {'
assert s.count(old)==1
s=s.replace(old,old+'''
    extern int zy_files_preflight(void),zy_terrain_prepare_files(void);
    extern void zy_console(const char *),zy_console_wait(void);
    extern void zy_store_begin(void);
    zy_store_begin();
    zy_console("Zynaps Wide - checking your original files...\\r\\n");
    #ifdef ZY_BOOT_DIAG
    extern void zy_boot_diag_step(void);zy_boot_diag_step();
    #endif
    if(zy_files_preflight() || !zy_terrain_prepare_files()) {
        zy_console("Copy the supported game files to this folder. Press a key.\\r\\n");
        zy_console_wait();zy_leave_supervisor(zy_saved_ssp);return;
    }
''')
old='    g_record[REC_PROGRAM_STAGED_BYTES] = (uint32_t)stage_program_image();'
assert s.count(old)==1
s=s.replace(old,old+'''
    if(g_record[REC_PROGRAM_STAGED_BYTES] != PROGRAM_BYTES) {
        zy_console("Unsupported or damaged ZYNAPS17.PRG. Press a key.\\r\\n");
        zy_console_wait();zy_leave_supervisor(zy_saved_ssp);return;
    }
''')
s=s.replace('    write_file(FILE_ANCHOR_BASE, &anchor_address, (long)sizeof anchor_address);','')
marker='    zy_cheats_resolve_scancodes(zy_image_base);'
assert s.count(marker)==1
s=s.replace(marker,'''    extern int zy_cache_prepare(uint8_t *),zy_store_attach(void);
    if(!zy_cache_prepare(zy_image_base) || !zy_store_attach()) {
        zy_console("Press a key to return.\\r\\n");zy_console_wait();
        zy_leave_supervisor(zy_saved_ssp);return;
    }
    write_file(FILE_ANCHOR_BASE, &anchor_address, (long)sizeof anchor_address);
'''+marker)
p.write_text(s)
