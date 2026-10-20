#ifndef PET_MODEL_H
#define PET_MODEL_H
#include <stdint.h>
#include <stddef.h>
typedef enum { PET_IDLE, PET_THINKING, PET_WORKING, PET_SPEAKING, PET_ATTENTION, PET_SLEEPING, PET_ERROR, PET_STATE_COUNT } pet_state;
typedef struct { pet_state requested, current; uint32_t changed; int connected; } pet_model;
int pet_state_parse(const char *name, pet_state *out);
void pet_model_event(pet_model *m, pet_state state);
int pet_model_tick(pet_model *m, uint32_t now);
// WHP2: blend indexed RGB565 + 8-bit alpha PackBits planes over prefilled dst.
int pet_frame_decode(const uint8_t *src, size_t size, const uint8_t *palette, uint16_t *dst, size_t pixels);
#endif
