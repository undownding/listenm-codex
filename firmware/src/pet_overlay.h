#ifndef PET_OVERLAY_H
#define PET_OVERLAY_H
#include <stddef.h>
#include <stdint.h>
/* Render-task owned; files are opened once at startup. */
void pet_overlay_init(size_t whale_size,size_t click_size);
void pet_overlay_close(void);
int pet_overlay_background(uint16_t *pixels);
int pet_overlay_read_click(size_t offset,void *data,size_t size);
/* Decode atomically: on failure pixels remain unchanged. */
int pet_overlay_frame(int click,size_t palette,size_t begin,size_t end,uint16_t *pixels);
unsigned pet_overlay_extra_open(const char *name);
void pet_overlay_extra_close(void);
int pet_overlay_extra_frame(unsigned index,uint16_t *pixels);
#endif
