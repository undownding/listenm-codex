#include "pet_pack.h"
#include "pet_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    uint16_t pixels[208*196]={0};
    assert(argc>1);
    for(int a=1;a<argc;a++) {
        FILE *f=fopen(argv[a],"rb"); assert(f); fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
        uint8_t *p=malloc(n); assert(p && fread(p,1,n,f)==(size_t)n); fclose(f);
        assert(!pet_pack_validate(p,n)); assert(pet_pack_validate(p,n-1));
        unsigned count=pet_u32(p+4); uint8_t *base=p+16+4*(count+1);
        for(unsigned i=0;i<count;i++) {
            unsigned begin=pet_u32(p+16+4*i),end=pet_u32(p+20+4*i);
            assert(!pet_frame_decode(base+begin,end-begin,base,pixels,208*196));
        }
        p[4]=255;p[5]=255; assert(pet_pack_validate(p,n)); free(p);
    }
    printf("Validated %d packs and every frame\n",argc-1); return 0;
}
