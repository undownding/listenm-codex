#include "pet_button.h"
int pet_button_sample(pet_button *b,int pressed,uint32_t now) {
    if(pressed!=b->raw) { b->raw=pressed; b->changed=now; }
    if(pressed==b->stable || (uint32_t)(now-b->changed)<20) return 0;
    b->stable=pressed;
    if(pressed) { b->armed=1; return 0; }
    if(!b->armed) return 0;
    b->armed=0;
    if(b->has_click && (uint32_t)(now-b->last_click)<320) { b->has_click=0; return 2; }
    b->has_click=1; b->last_click=now; return 1;
}
