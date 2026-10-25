#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "lisa_wifi.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "net_al.h"
#include "net_def.h"
#include "pet_config.h"
#include "receiver.h"
#include "pet_ui.h"
#include "pet_storage.h"

#include "pet_voice.h"
#include <board.h>
#include <lisa_device.h>
#include <lisa_gpio.h>

#define WIFI_READY 1
#define IP_READY 2
static EventGroupHandle_t network;
static mac_manager_t *mac;
static int8_t get_mac(uint8_t address[6]) { return mac_manager_get(mac, address, 6); }
static void wifi_ready(void) { xEventGroupSetBits(network, WIFI_READY); }
static void dhcp(int vif, bool success, uint32_t ip, uint32_t mask, uint32_t gw, void *arg) {
    (void)vif; (void)mask; (void)gw; (void)arg;
    if (success) {
        printf("[pet] DHCP %u.%u.%u.%u\n", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24);
        xEventGroupSetBits(network, IP_READY);
    } else xEventGroupClearBits(network, IP_READY);
}
static void connection(wifi_mgr_connection_info_t *info, void *arg) {
    (void)arg;
    if (info->status == WIFI_MGR_STA_CONNECTED) {
        printf("[pet] Wi-Fi associated; starting DHCP\n");
        net_if_up(net_if_get(WIFI_VIF_STA_IDX)); ls_dhcpc_start(WIFI_VIF_STA_IDX);
    } else if (info->status == WIFI_MGR_STA_DISCONNECTED) {
        xEventGroupClearBits(network, IP_READY);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX); net_if_down(net_if_get(WIFI_VIF_STA_IDX));
    }
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    lisa_device_t *power=lisa_device_get("gpiob");
    if(!power || lisa_gpio_configure(power,POWER_EN_PIN,LISA_GPIO_CONFIG_OUTPUT_HIGH)) {
        printf("[pet] power hold initialization failed\n");
        return -1;
    }
#if CONFIG_BOARD_ARCS_MINI3
    extern int pet_usb_init(void);
    if(pet_usb_init()) return -1;
#endif
    printf("[pet] listenm receiver boot\n");
    if (pet_storage_init() || pet_config_load()) {
        printf("[pet] TF/config.ini missing or invalid; startup stopped\n");
        if(pet_ui_resource_error()) printf("[pet-ui] error screen task creation failed\n");
        return -1;
    }
    if (pet_ui_init()) { printf("[pet-ui] task creation failed\n"); return -1; }
    network = xEventGroupCreate();
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};
    mac = mac_manager_init(mac_manager_ops_get()->mem_ops, mac_manager_ops_get()->content_ops, &config);
    if (!network || !mac) return -1;
    net_dhcp_register_status_callback(dhcp, NULL);
    lisa_wifi_ops_t ops = {.custom_mac = get_mac, .init_done = wifi_ready};
    if (lisa_wifi_init(&ops)) return -1;
    xEventGroupWaitBits(network, WIFI_READY, pdFALSE, pdTRUE, portMAX_DELAY);
    if (wifi_mgr_init(wifi_mgr_ops_get()) || wifi_mgr_sta_enable()) return -1;
    wifi_mgr_sta_add_connection_cb(connection, NULL);
    wifi_mgr_sta_config_t ap = {0};
    memcpy(ap.ssid,PET_WIFI_SSID,strlen(PET_WIFI_SSID));
    memcpy(ap.pwd,PET_WIFI_PASSWORD,strlen(PET_WIFI_PASSWORD));
    if (pet_voice_init()) printf("[voice] initialization failed\n");
    static char cursor[96];
    for (;;) {
        if (!(xEventGroupGetBits(network) & IP_READY)) {
            printf("[pet] connecting Wi-Fi\n");
            wifi_mgr_sta_connect(&ap, true);
            xEventGroupWaitBits(network, IP_READY, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
        }
        if (xEventGroupGetBits(network) & IP_READY) {
            printf("[pet] connecting bridge %s:%d\n", PET_BRIDGE_IP, PET_BRIDGE_PORT);
            receiver_run(PET_BRIDGE_IP, PET_BRIDGE_PORT, PET_BRIDGE_TOKEN, cursor);
            printf("[pet] stream closed; retrying\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
