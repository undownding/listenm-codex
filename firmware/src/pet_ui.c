#include "pet_ui.h"
#include "pet_storage.h"
#include "pet_text.h"
#include "pet_resource_error.h"
#include "pet_model.h"
#include "pet_extra.h"
#include "pet_pack.h"
#include "pet_button.h"
#include "pet_overlay.h"
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

static unsigned click_counts[CLICK_DOUBLE];
static uint32_t (*click_offsets)[601];
static size_t click_bases[CLICK_DOUBLE];
static int listening;
static unsigned voice_level;
void pet_ui_listening(int active,unsigned level) {
    taskENTER_CRITICAL(); listening=active; voice_level=level; taskEXIT_CRITICAL();
}
static int click_index_init(void) {
    click_offsets=lisa_mem_alloc(CLICK_DOUBLE*sizeof(*click_offsets));
    if(!click_offsets) return -1;
    for(unsigned i=0;i<CLICK_DOUBLE;i++) {
        if(!click_assets[i].size) continue;
        uint8_t header[16],table[601*4];
        size_t offset=click_assets[i].offset,size=click_assets[i].size;
        if(pet_overlay_read_click(offset,header,16) || memcmp(header,"WHP2",4) ||
           pet_u32(header+8)!=208 || pet_u32(header+12)!=196) return -1;
        unsigned count=pet_u32(header+4); if(!count || count>600) return -1;
        size_t base=16+4*(count+1);
        if(base+512>size || pet_overlay_read_click(offset+16,table,4*(count+1))) return -1;
        uint32_t prev=512;
        for(unsigned j=0;j<=count;j++) {
            uint32_t n=pet_u32(table+4*j);
            if((j==0 && n!=512) || (j && n<=prev) || n>size-base) return -1;
            click_offsets[i][j]=n; prev=n;
        }
        if(prev!=size-base) return -1;
        click_counts[i]=count; click_bases[i]=offset+base;
    }
    return 0;
}
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
static int present(lisa_device_t *display,uint16_t *screen) {
    lisa_display_buffer_desc_t desc={.width=240,.height=240,.buf_size=240*240*2};
    // Match the physical panel mounting while retaining full-screen DMA.
#ifdef CONFIG_BOARD_ARCS_MINI3
    for(unsigned i=0;i<240*240/2;i++) {
        uint16_t t=screen[i]; screen[i]=screen[240*240-1-i]; screen[240*240-1-i]=t;
    }
#else
    for(unsigned y=0;y<240;y++) for(unsigned x=y+1;x<240;x++) {
        uint16_t t=screen[y*240+x]; screen[y*240+x]=screen[x*240+y]; screen[x*240+y]=t;
    }
    for(unsigned y=0;y<240;y++) for(unsigned x=0;x<120;x++) {
        uint16_t t=screen[y*240+x]; screen[y*240+x]=screen[y*240+239-x]; screen[y*240+239-x]=t;
    }
#endif
    return lisa_display_write(display,0,0,&desc,screen);
}
static void render(void *force_error) {
    unsigned extra_count=0;
    lisa_device_t *display=NULL;
    int display_ready=0;
    uint8_t *extra=NULL;
    uint16_t *background=NULL;
    uint8_t *font=NULL; size_t font_size=0;
    uint16_t *screen=lisa_mem_alloc(240*240*2), *sprite=lisa_mem_alloc(WHALE_WIDTH*WHALE_HEIGHT*2);
    if(!screen || display_init(&display)) goto fail;
    display_ready=1;
    if(force_error || !sprite) goto fail;
    size_t click_size=0;
    for(unsigned i=0;i<sizeof(click_assets)/sizeof(click_assets[0]);i++)
        if(click_assets[i].offset+click_assets[i].size>click_size) click_size=click_assets[i].offset+click_assets[i].size;
    size_t whale_size=0;
    for(unsigned i=0;i<PET_STATE_COUNT;i++)
        if(whale_clips[i].frames[whale_clips[i].count]>whale_size) whale_size=whale_clips[i].frames[whale_clips[i].count];
    pet_overlay_init(whale_size,click_size);
    if(click_index_init()) goto fail;
    background=lisa_mem_alloc(240*240*2);
    if(pet_overlay_background(background)) goto fail;
    font=pet_storage_load("status-font.bin",PET_TEXT_SIZE,&font_size);
    if(pet_text_validate(font,font_size)) goto fail;
    const uint16_t *backdrop=background;
    size_t extra_size=0; unsigned extra_frame=0;
    int interaction=0,buffering=0; unsigned interaction_frame=0; uint32_t buffer_started=0;
    unsigned frame=0; pet_state previous=PET_STATE_COUNT; int previous_link=-1;
    TickType_t wake=xTaskGetTickCount();
    printf("[pet-ui] display ready 240x240 fps=%d\n",WHALE_FPS);
    for(;;) {
        taskENTER_CRITICAL();
        pet_model_tick(&model,(uint32_t)(xTaskGetTickCount()*portTICK_PERIOD_MS));
        pet_state state=model.current; int connected=model.connected;
        int capturing=listening; unsigned level=voice_level;
        if(capturing) state=PET_ATTENTION;
        int action=click_request; click_request=0;
        taskEXIT_CRITICAL();
        if(state!=previous) { frame=0; previous=state; printf("[pet-ui] animation=%s\n",whale_clips[state].name); }
        if(connected!=previous_link) { printf("[pet-ui] link=%s\n",connected?"online":"offline"); previous_link=connected; }
        uint32_t now=(uint32_t)(xTaskGetTickCount()*portTICK_PERIOD_MS);
        if(capturing) { interaction=0; buffering=0; }
        if(action && !capturing) {
            interaction=action==2?CLICK_DOUBLE:(int)next_click(); interaction_frame=0; buffering=0;
            printf("[pet-ui] click animation=%s\n",click_assets[interaction-1].name);
        }
        if(buffering && (uint32_t)(now-buffer_started)>=700) buffering=0;
        int idle=connected && !capturing && state==PET_IDLE && !interaction && !buffering;
        if(extra && !idle) { pet_overlay_extra_close(); lisa_mem_free(extra); extra=NULL; printf("[pet-ui] extra interrupted\n"); }
        uint8_t *loaded=pet_extra_take(idle && !extra,&extra_size);
        if(loaded) {
            extra_count=pet_overlay_extra_open((const char *)loaded);
            if(!extra_count) lisa_mem_free(loaded);
            else { extra=loaded; extra_frame=0; printf("[pet-ui] extra playing\n"); }
        }
        memcpy(screen,backdrop,240*240*2);
        for(unsigned y=0;y<WHALE_HEIGHT;y++) memcpy(sprite+y*WHALE_WIDTH,backdrop+(y+23)*240+16,WHALE_WIDTH*2);
        const whale_clip *clip=&whale_clips[buffering?PET_IDLE:state];
        uint32_t begin=clip->frames[frame],end=clip->frames[frame+1];
        if(interaction) {
            if(!click_assets[interaction-1].size) {
                const whale_clip *wave=&whale_clips[PET_SPEAKING];
                begin=wave->frames[interaction_frame]; end=wave->frames[interaction_frame+1];
                if(pet_overlay_frame(0,wave->palette,begin,end,sprite)) goto fail;
            } else {
                unsigned i=(unsigned)interaction-1;
                size_t base=click_bases[i];
                begin=click_offsets[i][interaction_frame]; end=click_offsets[i][interaction_frame+1];
                if(pet_overlay_frame(1,base,base+begin,base+end,sprite)) goto fail;
            }
        } else if(extra) {
            if(pet_overlay_extra_frame(extra_frame,sprite)) {
                pet_overlay_extra_close(); lisa_mem_free(extra); extra=NULL; continue;
            }
        } else if(pet_overlay_frame(0,clip->palette,begin,end,sprite)) goto fail;
        for(unsigned y=0;y<WHALE_HEIGHT;y++) memcpy(screen+(y+23)*240+16,sprite+y*WHALE_WIDTH,WHALE_WIDTH*2);
        unsigned label=capturing?PET_TEXT_LISTENING:interaction?PET_TEXT_CLICK:extra?PET_TEXT_EXTRA:
            buffering?PET_IDLE:connected?(unsigned)state:PET_TEXT_OFFLINE;
        pet_text_draw(font,font_size,PET_TEXT_BRAND,screen,PET_TEXT_TOP);
        pet_text_draw(font,font_size,label,screen,PET_TEXT_BOTTOM);
        if(capturing) {
            /* Live amplitude with a quiet breathing motion while waiting for speech. */
            unsigned amplitude=level/350; if(amplitude>12) amplitude=12;
            for(unsigned bar=0;bar<9;bar++) {
                unsigned height=3+amplitude+((frame+bar*2)%8);
                for(unsigned y=0;y<height;y++) for(unsigned x=0;x<4;x++)
                    screen[(PET_TEXT_BOTTOM-5-y)*240+94+bar*6+x]=INK;
            }
        }
        if(present(display,screen)) goto fail;
        if(extra && (++extra_frame)>=extra_count) { pet_overlay_extra_close(); lisa_mem_free(extra); extra=NULL; printf("[pet-ui] extra finished\n"); }
        if(interaction) {
            unsigned count=!click_assets[interaction-1].size?whale_clips[PET_SPEAKING].count:click_counts[interaction-1];
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
    size_t pending_size;
    lisa_mem_free(pet_extra_take(0,&pending_size));
    lisa_mem_free(extra);
    if(display_ready && !pet_resource_error_draw(screen,240*240) && !present(display,screen))
        printf("[pet-ui] resource missing screen shown\n");
    pet_overlay_close();
    lisa_mem_free(click_offsets); click_offsets=NULL;
    lisa_mem_free(font);
    if(background) lisa_mem_free(background);
    if(screen) lisa_mem_free(screen); if(sprite) lisa_mem_free(sprite);
    vTaskDelete(NULL);
}
int pet_ui_init(void) {
    if(started) return 0;
    if(xTaskCreate(render,"pet-ui",4096,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS) return -1;
    if(pet_extra_init()) printf("[pet-ui] extras unavailable\n");
    if(xTaskCreate(button_task,"pet-button",2048,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS)
        printf("[pet-ui] function key task unavailable\n");
    started=1; return 0;
}

int pet_ui_resource_error(void) {
    if(started) return -1;
    if(xTaskCreate(render,"pet-error",4096,(void *)1,tskIDLE_PRIORITY+1,NULL)!=pdPASS) return -1;
    started=1; return 0;
}
