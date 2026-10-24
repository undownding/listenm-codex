#ifndef PET_TEXT_H
#define PET_TEXT_H
#include <stdint.h>
#include <stddef.h>
enum { PET_TEXT_OFFLINE=7,PET_TEXT_CLICK,PET_TEXT_EXTRA,PET_TEXT_LISTENING,PET_TEXT_BRAND,PET_TEXT_COUNT };
#define PET_TEXT_MARGIN 8
#define PET_TEXT_TOP PET_TEXT_MARGIN
#define PET_TEXT_BOTTOM (240-PET_TEXT_MARGIN-20)
#define PET_TEXT_SIZE (16+PET_TEXT_COUNT*96*20)
int pet_text_validate(const uint8_t *font,size_t size);
int pet_text_draw(const uint8_t *font,size_t size,unsigned label,uint16_t *screen,unsigned y);
#endif
