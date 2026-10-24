/* First-run preparation uses the existing 512 KB game workspace. No second
 * workspace, game bytes, or generated sprite banks are linked into the app. */
#include "machine.h"
#include "init.h"
#include "mothership.h"
#include "cache_format.h"
#include "original_prg.h"
#include "cache_validate.h"
extern long Fopen(const char *,short),Fread(short,long,void *),Fclose(short);
extern long Fcreate(const char *,short),Fwrite(short,long,const void *);
extern long zy_fseek(long,short,short),zy_fdelete(const char *);
extern long zy_frename(const char *,const char *);
extern void zy_console(const char *);
extern const uint32_t zy_original_crc;
int zy_setup_running;
static int io_failed;
static short cache_handle=-1,tile_handle[3];
static uint8_t saved_text[ZY_ORIGINAL_TEXT],header[ZY_CACHE_HEADER];
static const char *failed_operation;
static long failed_result;
static int failure(const char *operation,long result){
    if(!failed_operation){failed_operation=operation;failed_result=result;}
    return 0;
}
static void report_failure(void){
    if(!failed_operation)return;
    char code[9] __attribute__((aligned(2)));
    uint32_t v=(uint32_t)failed_result;
    for(unsigned i=0;i<8;i++){
        unsigned n=(v>>(28-i*4))&15;code[i]=(char)(n<10?'0'+n:'A'+n-10);
    }
    code[8]=0;zy_console("Operation: ");zy_console(failed_operation);
    zy_console("; GEMDOS result 0x");zy_console(code);zy_console("\r\n");
}
static uint32_t word(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int read_existing(void *context,uint32_t at,void *out,unsigned n){
    short h=*(short *)context;
    return zy_fseek((long)at,h,0)==(long)at && Fread(h,(long)n,out)==(long)n;
}
int zy_cache_read(uint32_t at,void *out,unsigned n){
    if(at>ZY_CACHE_CAPACITY || n>ZY_CACHE_CAPACITY-at)return failure("cache read bounds",-64);
    long r=zy_fseek((long)at,cache_handle,0);
    if(r!=(long)at)return failure("cache read seek",r);
    r=Fread(cache_handle,(long)n,out);
    return r==(long)n?1:failure("cache read",r);
}
int zy_cache_write(uint32_t at,const void *data,unsigned n){
    if(at>ZY_CACHE_CAPACITY || n>ZY_CACHE_CAPACITY-at)return failure("cache write bounds",-64);
    long r=zy_fseek((long)at,cache_handle,0);
    if(r!=(long)at)return failure("cache write seek",r);
    r=Fwrite(cache_handle,(long)n,data);
    return r==(long)n?1:failure("cache write",r);
}
int zy_cache_tile_read(unsigned bank,uint32_t at,void *out,unsigned n){
    return bank<3 && tile_handle[bank]>=0 &&
        zy_fseek((long)at,tile_handle[bank],0)==(long)at &&
        Fread(tile_handle[bank],(long)n,out)==(long)n;
}
void zy_setup_load_file(uint8_t *im,uint32_t name,uint32_t dst,uint32_t count){
    char file[64] __attribute__((aligned(2)));unsigned i=0;
    if(name>=0x80000 || dst>0x80000 || count>0x80000-dst){io_failed=1;return;}
    while(name<0x80000 && im[name] && i<sizeof file-1){
        unsigned c=im[name++];if(c>='a'&&c<='z')c-=32;file[i++]=(char)c;
    }
    if(name>=0x80000 || im[name]){io_failed=1;return;}file[i]=0;
    long h=Fopen(file,0);if(h<0){io_failed=1;return;}
    /* The original intentionally requests more bytes than some files contain. */
    if(Fread((short)h,(long)count,im+dst)<0)io_failed=1;
    if(Fclose((short)h)<0)io_failed=1;
}
static int cached(void){
    long h=Fopen("ZYWCACHE.BIN",0);if(h<0)return 0;
    long n=Fread((short)h,sizeof header,header);
    long size=zy_fseek(0,(short)h,2);
    short handle=(short)h;
    int valid=size>0 && zy_cache_validate(read_existing,&handle,(uint32_t)size);
    Fclose(handle);
    static const char magic[]="ZYWCACHE";
    if(n!=sizeof header)return 0;
    for(unsigned i=0;i<8;i++)if(header[i]!=(uint8_t)magic[i])return 0;
    return valid && word(header+ZY_H_VERSION)==1 && word(header+ZY_H_ORIGINAL_CRC)==zy_original_crc &&
        size==(long)word(header+ZY_H_SIZE) && size>256 && size<=ZY_CACHE_CAPACITY &&
        word(header+ZY_H_HEADER_CRC)==(zy_crc32(0xffffffffu,header,80)^0xffffffffu);
}
int zy_cache_prepare(uint8_t *image){
    static const char *tiles[3]={"ZYN1.DAT","ZYN3.DAT","ZYN8.DAT"};
    if(cached())return 1;
    failed_operation=0;failed_result=0;
    zy_console("First launch: preparing graphics on your SD. Please wait.\r\n");
    for(unsigned i=0;i<ZY_ORIGINAL_TEXT;i++)saved_text[i]=image[0x10000+i];
    for(unsigned i=0;i<3;i++)tile_handle[i]=-1;
    long h=Fcreate("ZYWCACHE.TMP",0);int ok=h>=0;cache_handle=(short)h;
    if(!ok)failure("cache create",h);
    for(unsigned i=0;i<3 && ok;i++){h=Fopen(tiles[i],0);tile_handle[i]=(short)h;if(h<0)ok=0;}
    io_failed=0;zy_setup_running=1;
    if(ok)ok=zy_cache_builder_begin();
    for(unsigned section=0;section<16 && ok;section++){
        /* Reloading the base resources trades first-launch time for 512 KB of
         * saved RAM. It is never repeated during normal gameplay. */
        for(unsigned i=0;i<0x80000;i++)image[i]=0;
        for(unsigned i=0;i<ZY_ORIGINAL_TEXT;i++)image[0x10000+i]=saved_text[i];
        boot_load_title_assets(image);boot_load_gameplay_assets(image);
        image[A_level_section]=(uint8_t)section;section_load_assets(image);
        ok=!io_failed && zy_cache_builder_scene(section*5,image);
        image[A_mothership_prep_stage]=1;
        for(unsigned stage=1;stage<=3 && ok;stage++){
            mothership_sprite_build_step(image);ok=zy_cache_builder_scene(section*5+stage,image);
        }
        if(ok){mothership_sprite_expand(image);mothership_sprite_preshift(image);
            ok=zy_cache_builder_scene(section*5+4,image);}
        zy_console(".");
    }
    if(ok)ok=!io_failed && zy_cache_builder_finish();
    zy_setup_running=0;
    if(cache_handle>=0 && Fclose(cache_handle)<0)ok=0;
    cache_handle=-1;
    for(unsigned i=0;i<3;i++)if(tile_handle[i]>=0)Fclose(tile_handle[i]);
    /* Publish the finished header last, then rename the closed temporary file.
     * A failed/interrupted build leaves original user files untouched. */
    if(ok){zy_fdelete("ZYWCACHE.BIN");ok=zy_frename("ZYWCACHE.TMP","ZYWCACHE.BIN")==0;}
    for(unsigned i=0;i<0x80000;i++)image[i]=0;
    for(unsigned i=0;i<ZY_ORIGINAL_TEXT;i++)image[0x10000+i]=saved_text[i];
    zy_console(ok?"\r\nGraphics ready.\r\n":"\r\nPreparation failed.\r\n");
    if(!ok)report_failure();
    return ok;
}
