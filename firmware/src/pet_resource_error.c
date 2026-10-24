#include "pet_resource_error.h"
#include "resource_missing.h"
int pet_resource_error_draw(uint16_t *screen,size_t pixels) {
    if(!screen || pixels!=240*240) return -1;
    size_t used=0;
    for(size_t i=0;i<sizeof(pet_resource_missing_runs)/sizeof(pet_resource_missing_runs[0]);i++) {
        unsigned count=pet_resource_missing_runs[i][0];
        if(!count || count>pixels-used) return -1;
        while(count--) screen[used++]=pet_resource_missing_runs[i][1];
    }
    return used==pixels?0:-1;
}
