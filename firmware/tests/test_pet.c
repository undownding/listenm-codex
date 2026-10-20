#include "pet_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "whale.h"
int main(int argc,char **argv) {
    assert(argc==2);
    pet_model m={.connected=1}; pet_state s;
    assert(!pet_state_parse("thinking",&s) && s==PET_THINKING);
    assert(pet_state_parse("unknown",&s)==-1);
    pet_model_event(&m,PET_WORKING); assert(pet_model_tick(&m,1000));
    pet_model_event(&m,PET_THINKING); assert(!pet_model_tick(&m,1200));
    assert(pet_model_tick(&m,1700));
    pet_model_event(&m,PET_THINKING); assert(!pet_model_tick(&m,2000));
    assert(m.changed==1700); // same state never restarts animation
    pet_model_event(&m,PET_ERROR); assert(pet_model_tick(&m,2001));
    m.connected=0; assert(pet_model_tick(&m,2002) && m.current==PET_IDLE);
    FILE *f=fopen(argv[1],"rb"); assert(f);
    fseek(f,0,SEEK_END); long size=ftell(f); rewind(f);
    unsigned char *blob=malloc(size); assert(blob && fread(blob,1,size,f)==(size_t)size); fclose(f);
    uint16_t *pixels=calloc(WHALE_WIDTH*WHALE_HEIGHT,2); assert(pixels);
    for(unsigned c=0;c<PET_STATE_COUNT;c++) {
        const whale_clip *clip=&whale_clips[c];
        for(unsigned n=0;n<clip->count;n++) {
            unsigned start=clip->frames[n],end=clip->frames[n+1];
            assert(end<=(unsigned)size && start<end);
            assert(!pet_frame_decode(blob+start,end-start,blob+clip->palette,pixels,WHALE_WIDTH*WHALE_HEIGHT));
            assert(pet_frame_decode(blob+start,end-start-1,blob+clip->palette,pixels,WHALE_WIDTH*WHALE_HEIGHT)==-1);
        }
    }
    // Transparent, opaque white, and half-transparent white over black.
    unsigned char palette[512]={0}; palette[2]=255; palette[3]=255;
    unsigned char rgba[]={2,0,0,0, 130,1, 2,0,255,128};
    uint16_t blend[]={0x1234,0,0};
    assert(!pet_frame_decode(rgba,sizeof(rgba),palette,blend,3));
    assert(blend[0]==0x1234 && blend[1]==0xffff && blend[2]==0x8410);
    assert(pet_frame_decode(rgba,sizeof(rgba)-1,palette,blend,3)==-1);
    rgba[0]=255; assert(pet_frame_decode(rgba,sizeof(rgba),palette,blend,3)==-1);
    unsigned char bad[]={255,0};
    assert(pet_frame_decode(bad,sizeof(bad),blob,pixels,1)==-1);
    free(blob); free(pixels); puts("State transitions and all 840 animation frames passed");
}
