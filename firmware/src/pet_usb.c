/* USB ADB recovery remains available even when TF/config or voice setup fails. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <chip.h>
#include <sys/chip_id.h>
#include <usbd_core.h>
#include <adb.h>
#include <adb_device.h>
#include <adb_reboot.h>

#define USB_BUS 0
#ifdef CONFIG_USB_HS
#define BULK_SIZE 512
#else
#define BULK_SIZE 64
#endif
static const uint8_t device_desc[]={
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0,0,0,0,0x0483,0x0adb,0x0108,1)
};
static const uint8_t config_desc[]={
    USB_CONFIG_DESCRIPTOR_INIT(32,1,1,USB_CONFIG_BUS_POWERED,100),
    USB_INTERFACE_DESCRIPTOR_INIT(0,0,2,0xff,0x42,1,4),
    USB_ENDPOINT_DESCRIPTOR_INIT(0x01,2,BULK_SIZE,0),
    USB_ENDPOINT_DESCRIPTOR_INIT(0x81,2,BULK_SIZE,0)
};
static const uint8_t qualifier_desc[]={USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0,0,0,0,1)};
static char serial_number[17];
static const char *strings[]={"\x09\x04","ListenAI",CONFIG_BOARD_NAME,serial_number,"ADB Interface"};
static const uint8_t *device_descriptor(uint8_t speed) {(void)speed;return device_desc;}
static const uint8_t *config_descriptor(uint8_t speed) {(void)speed;return config_desc;}
static const uint8_t *qualifier_descriptor(uint8_t speed) {(void)speed;return qualifier_desc;}
static const char *string_descriptor(uint8_t speed,uint8_t index) {
    (void)speed;return index<sizeof(strings)/sizeof(*strings)?strings[index]:NULL;
}
static const struct usb_descriptor descriptors={
    .device_descriptor_callback=device_descriptor,
    .config_descriptor_callback=config_descriptor,
    .device_quality_descriptor_callback=qualifier_descriptor,
    .string_descriptor_callback=string_descriptor
};
static struct usbd_interface adb_interface;
static void usb_event(uint8_t bus,uint8_t event) {(void)bus;(void)event;}

/* Same retained boot-info ABI and full reset sequence as the SDK Mini3 app. */
void sys_platform_recovery(void) {
    usbd_deinitialize(USB_BUS);
    vTaskDelay(pdMS_TO_TICKS(300));
    uint32_t cause=IP_AON_CTRL->REG_SYSRST_STATUS.all;
    IP_AON_CTRL->REG_SYSRST_STATUS.all=cause;
    (void)IP_AON_CTRL->REG_SYSRST_STATUS.all;
    uint32_t info=IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    info &= ~(0x0000ffffu | (7u<<29));
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all=info|0x80000100u;
    (void)IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    __RWMB();
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0xfu<<21);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0xeu<<21);
    (void)IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all;
    __RWMB();
    for(;;) {
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN=1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN=1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN=1;
        __RWMB();
        IP_SYSCTRL->REG_SW_RESET_CP0.all=0xcafe000au;
        __RWMB();
    }
}
int pet_usb_init(void) {
    memcpy(serial_number,sys_chip_id_get(),sizeof(serial_number));
    serial_number[16]=0;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK=1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL=1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG=1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8=1;
    *(volatile uint8_t *)(USBC_BASE+1)&=~0x40u;
    *(volatile uint8_t *)(USBC_BASE+0x60)&=~1u;
    vTaskDelay(pdMS_TO_TICKS(300));
    usbd_desc_register(USB_BUS,&descriptors);
    usbd_add_interface(USB_BUS,adb_dev_init_intf(USB_BUS,&adb_interface,0x81,0x01));
    adb_init();adb_reboot_init();
    int rc=usbd_initialize(USB_BUS,USBC_BASE,usb_event);
    printf("[pet-usb] ADB recovery %s\n",rc?"initialization failed":"ready");
    return rc;
}
