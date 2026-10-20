#include "pet_pack.h"
#include <string.h>
uint32_t pet_u32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
int pet_pack_validate(const uint8_t *p,size_t n) {
    if(n<532 || memcmp(p,"WHP2",4) || pet_u32(p+8)!=208 || pet_u32(p+12)!=196) return -1;
    uint32_t count=pet_u32(p+4);
    if(!count || count>600) return -1;
    size_t base=16+4*(count+1);
    if(base+512>n || pet_u32(p+16)!=512) return -1;
    uint32_t prev=512;
    for(unsigned i=1;i<=count;i++) {
        uint32_t offset=pet_u32(p+16+4*i);
        if(offset<=prev || offset>n-base) return -1;
        prev=offset;
    }
    return prev==n-base?0:-1;
}
