#include "pet_ui.h"
#include "pet_model.h"
#include "pet_extra.h"
#include "pet_pack.h"
#include "pet_button.h"
#include "click.h"
#include <stdlib.h>
#include <lisa_gpio.h>
#include "whale.h"
#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include <FreeRTOS.h>
#include <task.h>
#include <lisa_device.h>
#include <lisa_display.h>
#include <lisa_mem.h>
#include <board.h>

extern const uint16_t pet_background[240*240];
#define INK LISA_DISPLAY_RGB565(57,76,113)
static pet_model model;
static int started;
static int click_request;
/* Shuffle a full pool before reuse; avoid repeats at the bag boundary too. */
static unsigned next_click(void) {
    static unsigned bag[CLICK_SINGLE_COUNT],remaining,last;
    if(!remaining) {
        for(unsigned i=0;i<CLICK_SINGLE_COUNT;i++) bag[i]=i+1;
        for(unsigned i=CLICK_SINGLE_COUNT-1;i>0;i--) {
            unsigned j=(unsigned)rand()%(i+1),t=bag[i]; bag[i]=bag[j]; bag[j]=t;
        }
        if(bag[CLICK_SINGLE_COUNT-1]==last) {
            unsigned t=bag[0]; bag[0]=bag[CLICK_SINGLE_COUNT-1]; bag[CLICK_SINGLE_COUNT-1]=t;
        }
        remaining=CLICK_SINGLE_COUNT;
    }
    last=bag[--remaining]; return last;
}
static void button_task(void *unused) {
    (void)unused;
    lisa_device_t *gpio=lisa_device_get("gpiob");
    if(!gpio || lisa_gpio_configure(gpio,4,LISA_GPIO_INPUT | LISA_GPIO_PULL_UP)) {
        printf("[pet-ui] function key unavailable\n"); vTaskDelete(NULL); return;
    }
    pet_button button={0};
    TickType_t wake=xTaskGetTickCount();
    for(;;) {
        int level=lisa_gpio_read_pin(gpio,4);
        int action=level<0?0:pet_button_sample(&button,level==0,(uint32_t)(xTaskGetTickCount()*portTICK_PERIOD_MS));
        if(action) {
            taskENTER_CRITICAL(); click_request=action; taskEXIT_CRITICAL();
            printf("[pet-ui] function key %s\n",action==2?"double":"single");
        }
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(10));
    }
}
// Minimal 5x7 uppercase font for the state labels; pixels are drawn with the frame.
static const unsigned char font[26][5]={
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {7,8,112,8,7},{97,81,73,69,67}};
static void text(uint16_t *buf,const char *s,int y) {
    int x=(240-(int)strlen(s)*6)/2;
    for(;*s;s++,x+=6) if(*s>='A' && *s<='Z')
        for(int col=0;col<5;col++) for(int row=0;row<7;row++)
            if(font[*s-'A'][col]&(1<<row)) buf[(y+row)*240+x+col]=INK;
}
void pet_ui_connected(int connected) {
    taskENTER_CRITICAL();
    model.connected=connected;
    if(!connected) model.requested=PET_IDLE;
    taskEXIT_CRITICAL();
}
void pet_ui_event(const char *json) {
    cJSON *root=cJSON_Parse(json);
    if(!root) return;
    cJSON *version=cJSON_GetObjectItemCaseSensitive(root,"version");
    cJSON *source=cJSON_GetObjectItemCaseSensitive(root,"source");
    cJSON *state=cJSON_GetObjectItemCaseSensitive(root,"state");
    pet_state next;
    if(cJSON_IsNumber(version) && version->valuedouble==1 && cJSON_IsString(source) &&
       !strcmp(source->valuestring,"codex") && cJSON_IsString(state) && !pet_state_parse(state->valuestring,&next)) {
        taskENTER_CRITICAL(); pet_model_event(&model,next); taskEXIT_CRITICAL();
    }
    cJSON_Delete(root);
}
static int display_init(lisa_device_t **out) {
    lisa_device_t *a=lisa_device_get("gpioa"), *b=lisa_device_get("gpiob");
    lisa_display_config_t cfg={
        .bus_type=LISA_DISPLAY_BUS_SPI_4WIRE,.panel_name="st7789p3",
        .bus_config.spi_4wire={.spi_dev=lisa_device_get("spi0"),.cs_gpio=a,.cs_pin=LCD_CS_PIN,
            .dc_gpio=a,.dc_pin=LCD_CD_PIN,.spi_freq=50000000},
        .backlight={.type=LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .config.pwm={.channel=1,.dev=lisa_device_get("pwm0"),.freq=2000}},
        .rst_gpio=b,.rst_pin=LCD_RST_PIN};
    *out=lisa_device_get("display");
    if(!a || !b || !*out || !cfg.bus_config.spi_4wire.spi_dev || !cfg.backlight.config.pwm.dev) return -1;
    if(lisa_display_attach_bus(*out,&cfg) || lisa_display_blanking_off(*out) ||
       lisa_display_set_brightness(*out,95)) return -1;
    return 0;
}
static void render(void *unused) {
    (void)unused;
    lisa_device_t *display;
    uint16_t *screen=lisa_mem_alloc(240*240*2), *sprite=lisa_mem_alloc(WHALE_WIDTH*WHALE_HEIGHT*2);
    if(!screen || !sprite || display_init(&display)) goto fail;
    uint8_t *extra=NULL; size_t extra_size=0; unsigned extra_frame=0;
    int interaction=0,buffering=0; unsigned interaction_frame=0; uint32_t buffer_started=0;
    unsigned frame=0; pet_state previous=PET_STATE_COUNT; int previous_link=-1;
    const char *labels[]={"IDLE","THINKING","WORKING","SPEAKING","WAITING","SLEEPING","ERROR"};
    lisa_display_buffer_desc_t desc={.width=240,.height=240,.buf_size=240*240*2};
    TickType_t wake=xTaskGetTickCount();
    printf("[pet-ui] display ready 240x240 fps=%d\n",WHALE_FPS);
    for(;;) {
        taskENTER_CRITICAL();
        pet_model_tick(&model,(uint32_t)(xTaskGetTickCount()*portTICK_PERIOD_MS));
        pet_state state=model.current; int connected=model.connected;
        int action=click_request; click_request=0;
        taskEXIT_CRITICAL();
        if(state!=previous) { frame=0; previous=state; printf("[pet-ui] animation=%s\n",whale_clips[state].name); }
        if(connected!=previous_link) { printf("[pet-ui] link=%s\n",connected?"online":"offline"); previous_link=connected; }
        uint32_t now=(uint32_t)(xTaskGetTickCount()*portTICK_PERIOD_MS);
        if(action) {
            interaction=action==2?CLICK_DOUBLE:(int)next_click(); interaction_frame=0; buffering=0;
            printf("[pet-ui] click animation=%s\n",click_assets[interaction-1].name);
        }
        if(buffering && (uint32_t)(now-buffer_started)>=700) buffering=0;
        int idle=connected && state==PET_IDLE && !interaction && !buffering;
        if(extra && !idle) { lisa_mem_free(extra); extra=NULL; printf("[pet-ui] extra interrupted\n"); }
        uint8_t *loaded=pet_extra_take(idle && !extra,&extra_size);
        if(loaded) {
            if(pet_pack_validate(loaded,extra_size)) lisa_mem_free(loaded);
            else { extra=loaded; extra_frame=0; printf("[pet-ui] extra playing\n"); }
        }
        memcpy(screen,pet_background,240*240*2);
        for(unsigned y=0;y<WHALE_HEIGHT;y++) memcpy(sprite+y*WHALE_WIDTH,pet_background+(y+23)*240+16,WHALE_WIDTH*2);
        const whale_clip *clip=&whale_clips[buffering?PET_IDLE:state];
        uint32_t begin=clip->frames[frame],end=clip->frames[frame+1];
        if(interaction) {
            if(!click_assets[interaction-1].size) {
                const whale_clip *wave=&whale_clips[PET_SPEAKING];
                begin=wave->frames[interaction_frame]; end=wave->frames[interaction_frame+1];
                if(pet_frame_decode(whale_blob+begin,end-begin,whale_blob+wave->palette,sprite,WHALE_WIDTH*WHALE_HEIGHT)) goto fail;
            } else {
                const uint8_t *pack=click_blob+click_assets[interaction-1].offset;
                const uint8_t *base=pack+16+4*(pet_u32(pack+4)+1);
                begin=pet_u32(pack+16+4*interaction_frame); end=pet_u32(pack+20+4*interaction_frame);
                if(pet_frame_decode(base+begin,end-begin,base,sprite,WHALE_WIDTH*WHALE_HEIGHT)) goto fail;
            }
        } else if(extra) {
            const uint8_t *base=extra+16+4*(pet_u32(extra+4)+1);
            begin=pet_u32(extra+16+4*extra_frame); end=pet_u32(extra+20+4*extra_frame);
            if(pet_frame_decode(base+begin,end-begin,base,sprite,WHALE_WIDTH*WHALE_HEIGHT)) {
                lisa_mem_free(extra); extra=NULL; continue;
            }
        } else if(end>(size_t)(whale_blob_end-whale_blob) || begin>end ||
           pet_frame_decode(whale_blob+begin,end-begin,whale_blob+clip->palette,sprite,WHALE_WIDTH*WHALE_HEIGHT)) goto fail;
        for(unsigned y=0;y<WHALE_HEIGHT;y++) memcpy(screen+(y+23)*240+16,sprite+y*WHALE_WIDTH,WHALE_WIDTH*2);
        text(screen,"CODEX",8); text(screen,interaction?"CLICK":extra?"EASTER EGG":buffering?"IDLE":connected?labels[state]:"OFFLINE",225);
        // The Arcs-Mini panel requires a clockwise framebuffer rotation. Rotate the
        // framebuffer in software so the driver keeps a simple full-screen DMA.
        for(unsigned y=0;y<240;y++) for(unsigned x=y+1;x<240;x++) {
            uint16_t t=screen[y*240+x]; screen[y*240+x]=screen[x*240+y]; screen[x*240+y]=t;
        }
        for(unsigned y=0;y<240;y++) for(unsigned x=0;x<120;x++) {
            uint16_t t=screen[y*240+x]; screen[y*240+x]=screen[y*240+239-x]; screen[y*240+239-x]=t;
        }
        if(lisa_display_write(display,0,0,&desc,screen)) goto fail;
        if(extra && ++extra_frame>=pet_u32(extra+4)) { lisa_mem_free(extra); extra=NULL; printf("[pet-ui] extra finished\n"); }
        if(interaction) {
            unsigned count=!click_assets[interaction-1].size?whale_clips[PET_SPEAKING].count:pet_u32(click_blob+click_assets[interaction-1].offset+4);
            if(++interaction_frame>=count) {
                interaction=0; buffering=1; buffer_started=now; frame=0;
                printf("[pet-ui] click finished; idle buffer\n");
            }
        }
        frame=(frame+1)%clip->count;
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(1000/WHALE_FPS));
    }
fail:
    printf("[pet-ui] display/asset failure\n");
    if(screen) lisa_mem_free(screen); if(sprite) lisa_mem_free(sprite);
    vTaskDelete(NULL);
}
int pet_ui_init(void) {
    if(started) return 0;
    for(unsigned i=0;i<sizeof(click_assets)/sizeof(click_assets[0]);i++)
        if(click_assets[i].size && pet_pack_validate(click_blob+click_assets[i].offset,click_assets[i].size)) return -1;
    if(xTaskCreate(render,"pet-ui",4096,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS) return -1;
    if(pet_extra_init()) printf("[pet-ui] extras unavailable\n");
    if(xTaskCreate(button_task,"pet-button",2048,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS)
        printf("[pet-ui] function key task unavailable\n");
    started=1; return 0;
}
