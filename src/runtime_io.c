/* Original game files remain on the user's SD. No asset arrays are linked. */
#include <stdint.h>
#include "original_prg.h"
extern long __real_Fopen(const char *,short),__real_Fread(short,long,void *),__real_Fclose(short);
extern void zy_console(const char *);
extern const uint32_t zy_original_crc;
typedef struct {const char *name;uint32_t size,crc;} zy_file;
extern const zy_file zy_required_files[61];
static short original_handle=-1;
static uint8_t header[28],relocations[8192],scratch[512];
static int original_read;
/* Older GEMDRIVE .Fread_core repeats the original request on its last
 * iteration instead of the remaining count. Keep every real trap within
 * its 4 KB transfer window, including imports ending before file EOF. */
static long bounded_read(short handle,long bytes,void *out){
    uint8_t *destination=out;long received=0;
    if(bytes<0)return -64;
    while(received<bytes){
        long n=bytes-received;if(n>4096)n=4096;
        long count=__real_Fread(handle,n,destination+received);
        if(count<0)return received?received:count;
        if(count>n)return -66;
        received+=count;
        if(count<n)break;
    }
    return received;
}
static int equal(const char *a,const char *b){
    while(*a && *b){unsigned c=(unsigned char)*a++;if(c>='a'&&c<='z')c-=32;if(c!=(unsigned char)*b++)return 0;}
    return !*a && !*b;
}
static const char *basename(const char *s){const char *b=s;while(*s){if(*s=='/'||*s=='\\'||*s==':')b=s+1;s++;}return b;}
static long open_original(void){
    long h=__real_Fopen("ZYNAPS17.PRG",0);
    if(h<0)h=__real_Fopen("AUTO\\ZYNAPS17.PRG",0);
    return h;
}
long __wrap_Fopen(const char *name,short mode){
    if(mode || !equal(basename(name),"ZYNAPS.IMG"))return __real_Fopen(name,mode);
    if(original_handle>=0)return -36;
    long h=open_original();if(h<0)return h;
    if(bounded_read((short)h,28,header)!=28 || !zy_original_header(header)){
        __real_Fclose((short)h);return -66;
    }
    original_handle=(short)h;original_read=0;return 0x7ffe;
}
long __wrap_Fread(short h,long n,void *out){
    if(h!=0x7ffe)return bounded_read(h,n,out);
    if(original_handle<0 || original_read || n!=ZY_ORIGINAL_TEXT)return -64;
    if(bounded_read(original_handle,n,out)!=n)return -66;
    long count=bounded_read(original_handle,sizeof relocations,relocations);
    if(count<4 || count==(long)sizeof relocations)return -66;
    uint32_t crc=zy_crc32(0xffffffffu,header,28);
    crc=zy_crc32(crc,out,ZY_ORIGINAL_TEXT);crc=zy_crc32(crc,relocations,(unsigned)count)^0xffffffffu;
    if(crc!=zy_original_crc || !zy_original_relocate(out,relocations,(unsigned)count))return -66;
    original_read=1;return n;
}
long __wrap_Fclose(short h){
    if(h!=0x7ffe)return __real_Fclose(h);
    if(original_handle<0)return -37;
    long result=__real_Fclose(original_handle);original_handle=-1;return result;
}
int zy_files_preflight(void){
    int failures=0;
    for(unsigned i=0;i<61;i++){
        const zy_file *f=zy_required_files+i;
        #ifdef ZY_BOOT_DIAG
        extern void zy_boot_diag_step(void);zy_boot_diag_step();
        #endif
        long h=__real_Fopen(f->name,0);uint32_t size=0,crc=0xffffffffu;
        if(h>=0){
            long n;
            while((n=__real_Fread((short)h,sizeof scratch,scratch))>0){
                size+=(uint32_t)n;crc=zy_crc32(crc,scratch,(unsigned)n);
                if(size>f->size)break;
            }
            __real_Fclose((short)h);
            if(n>=0 && size==f->size && (crc^0xffffffffu)==f->crc)continue;
            zy_console("Incorrect file: ");
        }else zy_console("Missing file: ");
        zy_console(f->name);zy_console("\r\n");failures++;
    }
    long original=open_original();
    if(original<0){zy_console("Missing file: ZYNAPS17.PRG (or AUTO\\ZYNAPS17.PRG)\r\n");failures++;}
    else __real_Fclose((short)original);
    return failures?-1:0;
}
