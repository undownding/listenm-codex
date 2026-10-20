#ifndef PET_PACK_H
#define PET_PACK_H
#include <stddef.h>
#include <stdint.h>
uint32_t pet_u32(const uint8_t *p);
int pet_pack_validate(const uint8_t *p,size_t n);
#endif
