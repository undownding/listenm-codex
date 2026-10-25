#ifndef PET_CONFIG_H
#define PET_CONFIG_H
#include <stddef.h>
typedef struct {
    char bridge_ip[254],bridge_token[256],wifi_ssid[32],wifi_password[64];
    char wake_word[128],wake_model[96],wake_wrap[96];
    unsigned bridge_port,voice_threshold;
} pet_config;
extern pet_config pet_settings;
int pet_config_parse(const char *data,size_t size,pet_config *out);
int pet_config_load(void);
#define PET_BRIDGE_IP pet_settings.bridge_ip
#define PET_BRIDGE_PORT pet_settings.bridge_port
#define PET_BRIDGE_TOKEN pet_settings.bridge_token
#define PET_WIFI_SSID pet_settings.wifi_ssid
#define PET_WIFI_PASSWORD pet_settings.wifi_password
#define PET_VOICE_THRESHOLD pet_settings.voice_threshold
#endif
