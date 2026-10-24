"""Isolate first-run asset building from real hardware and game file state."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
p=root/'src/init.c';s=p.read_text()
start=s.index('void boot_load_title_assets(uint8_t *image) {')
end=s.index('\n}',start)+2
body=s[start:end]
for call in ('shifter_select_low_resolution();',
             'sound_start(image, BOOT_TITLE_TUNE, BOOT_SOUND_CHANNEL_FROM_FCLOSE);',
             'screen_flip_buffers(image);','set_palette_title(image);'):
    assert body.count(call)==1,call
    body=body.replace(call,'if (!zy_setup_running) '+call)
s=s[:start]+body+s[end:]
s='extern int zy_setup_running;\n'+s
p.write_text(s)
p=root/'src/fileio.c';s=p.read_text()
marker='void load_file(uint8_t *image, uint32_t name, uint32_t destination, uint32_t length) {'
assert s.count(marker)==1
s=s.replace(marker,marker+'''
    extern int zy_setup_running;
    extern void zy_setup_load_file(uint8_t *,uint32_t,uint32_t,uint32_t);
    if(zy_setup_running){zy_setup_load_file(image,name,destination,length);return;}
''')
p.write_text(s)
