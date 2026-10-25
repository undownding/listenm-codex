#include <stdint.h>
_Static_assert(CONFIG_CP_FIRMWARE_COMPAT_OFFSET == 0x180, "Mini3 AP contract offset");
_Static_assert(CONFIG_MEM_PSRAM_BASE == 0x28300000, "Mini3 CP base");
_Static_assert(CONFIG_MEM_PSRAM_SIZE == 0x00500000, "Mini3 CP size");
__attribute__((section(".cp_firmware_compat"),used,aligned(4)))
const uint32_t pet_cp_psram[2]={CONFIG_MEM_PSRAM_BASE,CONFIG_MEM_PSRAM_SIZE};
