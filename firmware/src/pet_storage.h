#ifndef PET_STORAGE_H
#define PET_STORAGE_H
#include <stddef.h>
int pet_storage_init(void);
/* Caller owns returned buffer, including a spare NUL byte. */
void *pet_storage_load(const char *name,size_t limit,size_t *size);
#endif
