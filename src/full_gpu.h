#ifndef ZY_FULL_GPU_H
#define ZY_FULL_GPU_H
#include <stdint.h>
#ifdef ZY_FULL_GPU
void zy_gpu_begin(uint8_t *image);
void zy_gpu_flush(void);
void zy_gpu_present(void);
void zy_gpu_reset(void);
void zy_gpu_abort(void);
int zy_gpu_clear(uint8_t *image);
int zy_gpu_defer(uint8_t *image,uint32_t page,uint32_t screen,unsigned phase);
int zy_gpu_native_background(void);
int zy_gpu_collecting(void);
#else
#define zy_gpu_begin(image) ((void)0)
#define zy_gpu_flush() ((void)0)
#define zy_gpu_present() ((void)0)
#define zy_gpu_reset() ((void)0)
#define zy_gpu_abort() ((void)0)
#define zy_gpu_clear(image) 0
#define zy_gpu_defer(image,page,screen,phase) 0
#define zy_gpu_native_background() 0
#define zy_gpu_collecting() 0
#endif
#endif
