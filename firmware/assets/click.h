#ifndef PET_CLICK_ASSETS_H
#define PET_CLICK_ASSETS_H
#include <stdint.h>
extern const uint8_t click_blob[];
#define CLICK_SINGLE_COUNT 5
#define CLICK_DOUBLE 6
typedef struct { uint32_t offset,size; const char *name; } click_asset;
static const click_asset click_assets[] = {
 {0,1135382,"happy_hop"},
 {1135382,1101494,"shy_surprise"},
 {2236876,1005544,"tsundere_pout"},
 {3242420,1105830,"ticklish_giggle"},
 {0,0,"cheerful_wave"},
 {4348250,1176529,"blue_whale_appears"},
};
#endif
