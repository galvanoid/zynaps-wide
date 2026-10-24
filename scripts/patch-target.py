"""Fix the private GEMDRIVE copy without modifying upstream/vendor."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'gemdrive.s';s=p.read_text()
marker='.Fread_core_loop:\n\tsend_sync CMD_READ_BUFF_CALL, 12'
assert s.count(marker)==1,'Unexpected GEMDRIVE Fread implementation'
s=s.replace(marker,'.Fread_core_loop:\n\n\tmove.l\td5, d4\t; remaining bytes, including the final partial chunk\n\tsend_sync CMD_READ_BUFF_CALL, 12')
marker='.Frename:\n\tmove.l\t10(a0), a5\n\tmove.l\t14(a0), a6\n\n\tdetect_emulated_drive_letter'
assert s.count(marker)==1,'Unexpected GEMDRIVE Frename implementation'
s=s.replace(marker,'.Frename:\n\tmove.l\t10(a0), a5\n\tmove.l\t14(a0), a6\n\tmove.l\ta5, a4\t; drive detection uses the source filename in a4\n\n\tdetect_emulated_drive_letter')
p.write_text(s,newline='\n')
