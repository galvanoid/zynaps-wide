#include "map_projection.h"
static uint16_t cells[7200],queue[7200];
static uint8_t seen[900];
static uint16_t word(const uint8_t *p){return (uint16_t)(((unsigned)p[0]<<8)|p[1]);}
static int occupied(unsigned x,unsigned y){return (cells[y*400+x]&0x7fffu)!=0;}
static int visited(unsigned x,unsigned y){unsigned i=y*400+x;return seen[i>>3]&(1u<<(i&7u));}
static void enqueue(unsigned x,unsigned y,unsigned *tail){
    unsigned i=y*400+x;seen[i>>3]|=(uint8_t)(1u<<(i&7u));queue[(*tail)++]=(uint16_t)((y<<9)|x);
}
int zy_project_map(const uint8_t *data,unsigned length,uint32_t output[400]){
    unsigned pos=0;
    for(unsigned row=0;row<18;row++){
        unsigned x=0;
        while(x<400){
            if(pos+2>length)return 0;
            unsigned token=word(data+pos);pos+=2;
            unsigned count=token&0x7fffu;
            if(!count || count>400-x)return 0;
            if(token&0x8000u){
                if(pos+2>length)return 0;
                unsigned tile=word(data+pos);pos+=2;
                while(count--)cells[row*400+x++]=(uint16_t)tile;
            }else{
                if(count>(length-pos)/2)return 0;
                while(count--){cells[row*400+x++]=word(data+pos);pos+=2;}
            }
        }
    }
    if(pos!=length)return 0;
    for(unsigned i=0;i<900;i++)seen[i]=0;
    for(unsigned i=0;i<400;i++)output[i]=0x3fe00u;
    for(unsigned y=0;y<18;y++)for(unsigned x=0;x<400;x++){
        if(!occupied(x,y)||visited(x,y))continue;
        unsigned head=0,tail=0,top=y,bottom=y;enqueue(x,y,&tail);
        while(head<tail){
            unsigned at=queue[head++],px=at&511u,py=at>>9;
            if(py<top)top=py;
            if(py>bottom)bottom=py;
            if(px && occupied(px-1,py)&&!visited(px-1,py))enqueue(px-1,py,&tail);
            if(px<399 && occupied(px+1,py)&&!visited(px+1,py))enqueue(px+1,py,&tail);
            if(py && occupied(px,py-1)&&!visited(px,py-1))enqueue(px,py-1,&tail);
            if(py<17 && occupied(px,py+1)&&!visited(px,py+1))enqueue(px,py+1,&tail);
        }
        for(unsigned i=0;i<tail;i++){
            unsigned px=queue[i]&511u,py=queue[i]>>9;
            if(top==0 && bottom==17){if(py==8 && occupied(px,9))output[px]|=1u<<18;}
            else if(top>0 && bottom>=9)output[px]|=1u<<py;
            else output[px]&=~(1u<<py);
        }
    }
    return 1;
}
