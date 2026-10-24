#ifndef PET_RESOURCE_ERROR_H
#define PET_RESOURCE_ERROR_H
#include <stddef.h>
#include <stdint.h>
/* Built-in emergency screen; independent of TF, background and status fonts. */
int pet_resource_error_draw(uint16_t *screen,size_t pixels);
#endif
