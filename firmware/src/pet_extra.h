#ifndef PET_EXTRA_H
#define PET_EXTRA_H
#include <stddef.h>
#include <stdint.h>
int pet_extra_init(void);
/* Called only by renderer. Ownership of returned allocation transfers to it. */
uint8_t *pet_extra_take(int enabled, size_t *size);
#endif
