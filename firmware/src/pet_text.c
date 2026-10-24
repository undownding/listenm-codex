#include "pet_text.h"
#include "pet_pack.h"
#include <string.h>
int pet_text_validate(const uint8_t *font,size_t size) {
    return !font || size!=PET_TEXT_SIZE || memcmp(font,"PTF1",4) ||
        pet_u32(font+4)!=PET_TEXT_COUNT || pet_u32(font+8)!=96 || pet_u32(font+12)!=20 ? -1:0;
}
int pet_text_draw(const uint8_t *font,size_t size,unsigned label,uint16_t *screen,unsigned y) {
    if(pet_text_validate(font,size) || !screen || label>=PET_TEXT_COUNT || y>220) return -1;
    const uint8_t *alpha=font+16+label*96*20;
    /* RGB565 ink (57,76,113), matching the existing navy labels. */
    for(unsigned row=0;row<20;row++) for(unsigned x=0;x<96;x++) {
        unsigned a=alpha[row*96+x],inv=255-a;
        unsigned bg=screen[(y+row)*240+72+x];
        unsigned r=(7*a+(bg>>11)*inv+127)/255;
        unsigned g=(19*a+((bg>>5)&63)*inv+127)/255;
        unsigned b=(14*a+(bg&31)*inv+127)/255;
        screen[(y+row)*240+72+x]=(uint16_t)((r<<11)|(g<<5)|b);
    }
    return 0;
}
