/* Public firmware provisions only our asset-free PRG. The private cache is
 * produced from the owner's SD files, validated in full, then kept in XIP. */
#include <string.h>
#include "ff.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "health.h"
#include "sdcard.h"
#include "app_store.h"
#include "cache_format.h"
#include "cache_validate.h"
#include "resource_cache.h"
#include "original_prg.h"
#include "server.h"
#include "program_bundle.h"
extern unsigned char __rom_in_ram_start__[];
extern zy_server zynaps_server;
static volatile unsigned requested;
#define shared ((volatile uint16_t *)&__rom_in_ram_start__)
#define scratch ((uint8_t *)&__rom_in_ram_start__+0x5000u)
#define cached_flash ((const uint8_t *)(0x10000000u+ZY_CACHE_FLASH_OFFSET))
_Static_assert(ZY_CACHE_FLASH_OFFSET+ZY_CACHE_CAPACITY==0x100000u,"Cache must end before ROM scratch and settings");
_Static_assert(ZY_CACHE_FLASH_OFFSET%FLASH_SECTOR_SIZE==0,"Cache alignment");
static int file_read(void *context,uint32_t at,void *out,unsigned n){
    FIL *file=context;UINT got;
    health_feed();
    return f_lseek(file,at)==FR_OK && f_read(file,out,n,&got)==FR_OK && got==n;
}
static int unpack(const uint8_t *data,unsigned bytes,uint8_t *out,unsigned expected){
    unsigned at=0,used=0;
    while(at<bytes){
        unsigned token=data[at++],literal=token>>4,match=(token&15)+4;
        if(literal==15){unsigned v;do{if(at>=bytes)return 0;v=data[at++];literal+=v;}while(v==255);}
        if(literal>bytes-at || literal>expected-used)return 0;
        memcpy(out+used,data+at,literal);at+=literal;used+=literal;
        if(at==bytes)return used==expected;
        if(bytes-at<2)return 0;
        unsigned distance=data[at]|((unsigned)data[at+1]<<8);at+=2;
        if(!distance || distance>used)return 0;
        if((token&15)==15){unsigned v;do{if(at>=bytes)return 0;v=data[at++];match+=v;}while(v==255);}
        if(match>expected-used)return 0;
        while(match--){out[used]=out[used-distance];used++;}
    }
    return used==expected;
}
static int program_present(void){
    FIL f;UINT got;uint32_t crc=0xffffffffu;
    if(f_open(&f,"/ZYNAPS/ZYWIDE.PRG",FA_READ)!=FR_OK)return 0;
    int ok=f_size(&f)==zy_program_size;
    while(ok){
        if(f_read(&f,scratch,512,&got)!=FR_OK){ok=0;break;}
        if(!got)break;
        crc=zy_crc32(crc,scratch,got);health_feed();
    }
    f_close(&f);return ok && (crc^0xffffffffu)==zy_program_crc;
}
static int provision_program(void){
    FIL f;UINT written;uint32_t crc=0xffffffffu,total=0;
    if(program_present())return 1;
    if(f_open(&f,"/ZYNAPS/ZYWIDE.NEW",FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)return 0;
    int ok=1;
    for(unsigned i=0;i<zy_program_blocks && ok;i++){
        const zy_program_block *b=zy_program_index+i;
        ok=b->raw<=8192 && unpack(zy_program_data+b->offset,b->packed,scratch,b->raw);
        if(ok){crc=zy_crc32(crc,scratch,b->raw);total+=b->raw;
            ok=f_write(&f,scratch,b->raw,&written)==FR_OK && written==b->raw;}
        health_feed();
    }
    if(f_close(&f)!=FR_OK)ok=0;
    if(!ok || total!=zy_program_size || (crc^0xffffffffu)!=zy_program_crc)return 0;
    FILINFO info;int old=f_stat("/ZYNAPS/ZYWIDE.PRG",&info)==FR_OK;
    if(old){
        f_unlink("/ZYNAPS/ZYWIDE.OLD");
        if(f_rename("/ZYNAPS/ZYWIDE.PRG","/ZYNAPS/ZYWIDE.OLD")!=FR_OK)return 0;
    }
    if(f_rename("/ZYNAPS/ZYWIDE.NEW","/ZYNAPS/ZYWIDE.PRG")!=FR_OK){
        if(old)f_rename("/ZYNAPS/ZYWIDE.OLD","/ZYNAPS/ZYWIDE.PRG");
        return 0;
    }
    return 1;
}
static void capabilities(int ready){
    static const unsigned offsets[]={ZY_TERRAIN_CAPS_OFFSET,ZY_FULL_CAPS_OFFSET,ZY_REVERSE_CAPS_OFFSET};
    static const uint16_t values[]={ZY_TERRAIN_CAPS_MAGIC,ZY_FULL_CAPS_MAGIC,ZY_REVERSE_CAPS_MAGIC};
    for(unsigned i=0;i<3;i++){
        shared[offsets[i]/2]=ready?values[i]:0;
        shared[offsets[i]/2+1]=ready?(uint16_t)~values[i]:0;
    }
}
static int install_cache(void){
    FIL f;
    if(f_open(&f,"/ZYNAPS/ZYWCACHE.BIN",FA_READ)!=FR_OK)return 0;
    uint32_t bytes=f_size(&f);
    if(!zy_cache_validate(file_read,&f,bytes)){f_close(&f);return 0;}
    /* Compare the complete verified cache. Matching data never wears flash. */
    int same=1;
    for(uint32_t at=0;at<bytes && same;){
        unsigned n=bytes-at;if(n>4096)n=4096;
        if(!file_read(&f,at,scratch,n)){f_close(&f);return 0;}
        same=!memcmp(cached_flash+at,scratch,n);at+=n;
    }
    if(!same){
        capabilities(0);
        unsigned rounded=(bytes+FLASH_SECTOR_SIZE-1)&~(FLASH_SECTOR_SIZE-1);
        if(rounded>ZY_CACHE_CAPACITY){f_close(&f);return 0;}
        /* There is no second CPU core in md-devops. IRQ code cannot execute
         * from XIP during erase/program. RP SDK flash routines run from RAM. */
        for(uint32_t at=0;at<rounded;at+=FLASH_SECTOR_SIZE){
            memset(scratch,0xff,FLASH_SECTOR_SIZE);
            unsigned n=bytes-at;if(n>FLASH_SECTOR_SIZE)n=FLASH_SECTOR_SIZE;
            if(!file_read(&f,at,scratch,n)){f_close(&f);return 0;}
            uint32_t irq=save_and_disable_interrupts();
            flash_range_erase(ZY_CACHE_FLASH_OFFSET+at,FLASH_SECTOR_SIZE);
            flash_range_program(ZY_CACHE_FLASH_OFFSET+at,scratch,FLASH_SECTOR_SIZE);
            restore_interrupts(irq);health_feed();
        }
    }
    f_close(&f);
    int ok=zy_resources_bind(cached_flash,ZY_CACHE_CAPACITY);
    capabilities(ok);return ok;
}
void zy_store_request(void){
    shared[ZY_STORE_STATUS_OFFSET/2]=ZY_STORE_BUSY;requested=1;
}
void zy_store_poll(void){
    if(!requested)return;
    requested=0;
    shared[ZY_STORE_STATUS_OFFSET/2]=install_cache()?ZY_STORE_READY:ZY_STORE_ERROR;
}
void zy_store_init(void){
    capabilities(0);
    shared[ZY_BOSS2X_CAPS_OFFSET/2]=shared[ZY_BOSS2X_CAPS_OFFSET/2+1]=0;
    shared[ZY_DIRTY_CAPS_OFFSET/2]=shared[ZY_DIRTY_CAPS_OFFSET/2+1]=0;
    shared[ZY_STORE_CAPS_OFFSET/2]=ZY_STORE_CAPS_MAGIC;
    shared[ZY_STORE_CAPS_OFFSET/2+1]=(uint16_t)~ZY_STORE_CAPS_MAGIC;
    shared[ZY_STORE_STATUS_OFFSET/2]=ZY_STORE_MISSING;
    if(!sdcard_isMounted())return;
    FRESULT r=f_mkdir("/ZYNAPS");if(r!=FR_OK && r!=FR_EXIST)return;
    if(!provision_program()){shared[ZY_STORE_STATUS_OFFSET/2]=ZY_STORE_ERROR;return;}
    /* No private data is required when the firmware is first installed. */
    if(install_cache())shared[ZY_STORE_STATUS_OFFSET/2]=ZY_STORE_READY;
}
