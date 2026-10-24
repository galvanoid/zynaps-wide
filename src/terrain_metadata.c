/* Layout is generated from the user's maps, never compiled into the PRG. */
#include "machine.h"
#include "scroll.h"
#include "init.h"
#include "map_projection.h"
extern long __real_Fopen(const char *,short),__real_Fread(short,long,void *),__real_Fclose(short);
static uint32_t columns[12][400];
static uint8_t data[16384];
int zy_terrain_prepare_files(void){
    static const char names[]="123456789XYZ";
    /* GCC may initialize a char array with long stores. Keep its stack
     * address even on a real 68000, including this nine-byte filename. */
    char file[9] __attribute__((aligned(2)))="LEV1.MAP";
    for(unsigned i=0;i<12;i++){
        file[3]=names[i];long h=__real_Fopen(file,0);if(h<0)return 0;
        long n=__real_Fread((short)h,sizeof data,data);__real_Fclose((short)h);
        if(n<0 || n==(long)sizeof data || !zy_project_map(data,(unsigned)n,columns[i]))return 0;
    }
    return 1;
}
int zy_terrain_metadata(const uint8_t *image,uint32_t map,uint8_t out[8]){
    static const char names[]="123456789XYZ";
    unsigned level,offset,column=0,name=image[A_filename_lev_map+3];
    if(name>='a' && name<='z')name-=32;
    for(level=0;level<12 && (unsigned)names[level]!=name;level++){}
    if(level==12 || map<A_map_unpacked)return 0;
    offset=map-A_map_unpacked;
    while(offset>=36 && column<421){offset-=36;column++;}
    if(offset || column>420)return 0;
    for(unsigned half=0;half<2;half++){
        uint32_t code=column+half<400?columns[level][column+half]:0x3fe00u;
        out[half*4]=(uint8_t)((code>>16)&3u);out[half*4+1]=(uint8_t)(code>>8);
        out[half*4+2]=(uint8_t)code;out[half*4+3]=(uint8_t)(code>>18);
    }
    return 1;
}
