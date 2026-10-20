#include "pet_button.h"
#include <assert.h>
#include <stdint.h>
static int click(pet_button *b,uint32_t at) {
    assert(!pet_button_sample(b,1,at));
    assert(!pet_button_sample(b,1,at+20));
    assert(!pet_button_sample(b,0,at+70));
    return pet_button_sample(b,0,at+90);
}
int main(void) {
    pet_button b={0};
    assert(!pet_button_sample(&b,0,0));
    assert(click(&b,100)==1);
    assert(click(&b,300)==2);
    assert(click(&b,500)==1); /* triple click starts another single */
    assert(click(&b,820)==1); /* exactly 320 ms is not double */
    assert(!pet_button_sample(&b,1,1000));
    assert(!pet_button_sample(&b,0,1005));
    assert(!pet_button_sample(&b,0,1030)); /* bounce must not click */
    assert(!pet_button_sample(&b,1,1100));
    assert(!pet_button_sample(&b,1,1120));
    assert(!pet_button_sample(&b,1,4000)); /* long press does not repeat */
    assert(!pet_button_sample(&b,0,4010));
    assert(pet_button_sample(&b,0,4030)==1);
    assert(!pet_button_sample(&b,0,5000));
    b=(pet_button){0};
    assert(click(&b,UINT32_MAX-200)==1);
    assert(click(&b,10)==2); /* tick rollover */
    return 0;
}
