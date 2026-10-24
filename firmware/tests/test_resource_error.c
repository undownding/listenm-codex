#include "pet_resource_error.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void) {
    uint16_t pixels[240*240+2];
    pixels[0]=0x1234; pixels[240*240+1]=0x5678;
    assert(pet_resource_error_draw(NULL,240*240)==-1);
    assert(pet_resource_error_draw(pixels+1,240*240-1)==-1);
    assert(!pet_resource_error_draw(pixels+1,240*240));
    assert(pixels[0]==0x1234 && pixels[240*240+1]==0x5678);
    unsigned different=0;
    for(unsigned i=1;i<=240*240;i++) different+=pixels[i]!=pixels[1];
    assert(different>3000 && different<20000);
    puts("Built-in resource error image decodes with complete bounded coverage");
}
