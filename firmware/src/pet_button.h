#ifndef PET_BUTTON_H
#define PET_BUTTON_H
#include <stdint.h>
typedef struct { int raw,stable,armed,has_click; uint32_t changed,last_click; } pet_button;
/* 0: no event, 1: single click, 2: double click; click occurs on release. */
int pet_button_sample(pet_button *b,int pressed,uint32_t now);
#endif
