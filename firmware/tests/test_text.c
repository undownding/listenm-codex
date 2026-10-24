#include "pet_text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==2); FILE *f=fopen(argv[1],"rb"); assert(f);
    uint8_t font[PET_TEXT_SIZE]; assert(fread(font,1,sizeof(font),f)==sizeof(font)); assert(fgetc(f)==EOF); fclose(f);
    assert(!pet_text_validate(font,sizeof(font)));
    uint16_t pixels[240*240],previous[240*240];
    for(unsigned label=0;label<PET_TEXT_COUNT;label++) {
        memset(pixels,0xff,sizeof(pixels));
        assert(!pet_text_draw(font,sizeof(font),label,pixels,PET_TEXT_BOTTOM));
        unsigned painted=0;
        for(unsigned y=0;y<240;y++) for(unsigned x=0;x<240;x++) {
            if(y<PET_TEXT_BOTTOM || y>=PET_TEXT_BOTTOM+20 || x<72 || x>=168) assert(pixels[y*240+x]==65535);
            else painted+=pixels[y*240+x]!=65535;
        }
        assert(painted>30);
        if(label) assert(memcmp(previous,pixels,sizeof(pixels)));
        memcpy(previous,pixels,sizeof(pixels));
    }
    memset(pixels,0xff,sizeof(pixels));
    assert(!pet_text_draw(font,sizeof(font),PET_TEXT_BRAND,pixels,PET_TEXT_TOP));
    for(unsigned i=0;i<PET_TEXT_MARGIN*240;i++) assert(pixels[i]==65535);
    assert(pet_text_draw(font,sizeof(font),PET_TEXT_COUNT,pixels,219)==-1);
    assert(pet_text_draw(font,sizeof(font),0,pixels,221)==-1);
    assert(pet_text_validate(font,sizeof(font)-1)==-1);
    font[0]=0; assert(pet_text_validate(font,sizeof(font))==-1);
    puts("Chinese status atlas, clipping, distinct labels and malformed font checks passed");
}
