#include "pet_voice.h"
#include "pet_config.h"
#include <FreeRTOS.h>
#include <task.h>
#include <lisa_mem.h>
#include <lisa_audio.h>
#include <lisa_device.h>
#include <acomp.h>
#include <acomp_wakeup.h>
#include <ic_message.h>
#include <cJSON.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_SAMPLES (16000*30)
/* Energy endpointing: 1 s silence after >= 200 ms speech; 5 s no-speech timeout. */
static int16_t *speech;
static unsigned used, quiet, voiced;
static int recording, pending, complete;
static TaskHandle_t upload_task;
static int send_all(int fd,const void *data,size_t size) {
    const char *p=data;
    while(size) { int n=send(fd,p,size,0); if(n<=0) return -1; p+=n; size-=n; }
    return 0;
}
static int post(const char *path,const void *body,size_t size,char *response,size_t capacity) {
    int fd=socket(AF_INET,SOCK_STREAM,0),result=-1;
    if(fd<0) return -1;
    struct timeval timeout={.tv_sec=90};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(PET_BRIDGE_PORT)};
    if(inet_pton(AF_INET,PET_BRIDGE_IP,&addr.sin_addr)!=1 || connect(fd,(void*)&addr,sizeof(addr))) goto done;
    char header[1024];
    int n=snprintf(header,sizeof(header),"POST %s HTTP/1.0\r\nHost: %s\r\nAuthorization: Bearer %s\r\nContent-Type: application/octet-stream\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",path,PET_BRIDGE_IP,PET_BRIDGE_TOKEN,(unsigned)size);
    if(n<0 || n>=(int)sizeof(header) || send_all(fd,header,n) || send_all(fd,body,size)) goto done;
    size_t count=0;
    while(count<capacity-1) {
        n=recv(fd,response+count,capacity-1-count,0);
        if(n<0) goto done;
        if(!n) break;
        count+=n;
    }
    response[count]=0;
    int status=0; sscanf(response,"HTTP/%*s %d",&status);
    if(status==200 && strstr(response,"\r\n\r\n")) result=0;
done:
    closesocket(fd); return result;
}
static void wake_event(uint32_t event,void *data,uint32_t len,void *priv) {
    (void)priv;
    if(!(event&WAKEUP_CB_EVENT_ENGINE_RLT) || !data || !len) return;
    cJSON *root=cJSON_ParseWithLength(data,len);
    cJSON *item=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"rlt"),0);
    cJSON *main=cJSON_GetObjectItemCaseSensitive(item,"bMain");
    if(cJSON_IsNumber(main) && main->valueint==1) {
        int notify=0;
        taskENTER_CRITICAL();
        if(!pending) { used=quiet=voiced=0; complete=0; pending=recording=1; notify=1; }
        taskEXIT_CRITICAL();
        if(notify) xTaskNotifyGive(upload_task);
    }
    cJSON_Delete(root);
}
static void audio_event(const lisa_audio_event_t *event,void *unused) {
    (void)unused;
    if(!event->record_buffer) return;
    const int16_t *pcm=event->record_buffer;
    /* Copy into algorithm-owned memory while the driver's buffer is still valid. */
    uint32_t capacity=0; uint16_t index=0;
    uint8_t *buffer=acomp_wakeup_stream_tx_buffer_alloc(0,&capacity,&index);
    unsigned samples=event->record_samples/2; /* SDK reports interleaved sample count. */
    if(buffer) {
        acomp_wakeup_audio_in_t *out=(void*)buffer;
        unsigned count=capacity/sizeof(*out);
        memset(buffer,0,capacity);
        for(unsigned i=0;i<samples && i<count;i++) { out[i].mic0=pcm[2*i]; out[i].ref0=pcm[2*i+1]; }
        acomp_wakeup_stream_tx_buffer_submit(0,buffer,capacity,index);
    }
    taskENTER_CRITICAL();
    if(recording) {
        unsigned energy=0;
        for(unsigned i=0;i<samples && used<MAX_SAMPLES;i++) {
            int value=pcm[2*i]; speech[used++]=(int16_t)value;
            energy+=(unsigned)(value<0?-value:value);
        }
        if(samples && energy/samples>=PET_VOICE_THRESHOLD) { voiced+=samples; quiet=0; }
        else quiet+=samples;
        if(used>=MAX_SAMPLES || (voiced>=3200 && quiet>=16000) || (voiced<3200 && used>=80000)) {
            recording=0; complete=1;
        }
    }
    taskEXIT_CRITICAL();
}
static void drain(void *unused) {
    (void)unused;
    for(;;) {
        uint32_t length; uint16_t index;
        uint8_t *p;
        while((p=acomp_wakeup_stream_rx_buffer_get(1,&length,&index)))
            acomp_wakeup_stream_rx_buffer_release(1,index,length,p);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void upload(void *unused) {
    (void)unused;
    for(;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        char response[2048],path[96],id[40]={0};
        int ok=!post("/voice/wake",NULL,0,response,sizeof(response));
        if(ok) {
            cJSON *root=cJSON_Parse(strstr(response,"\r\n\r\n")+4);
            cJSON *value=cJSON_GetObjectItemCaseSensitive(root,"id");
            if(cJSON_IsString(value) && strlen(value->valuestring)==36) memcpy(id,value->valuestring,37);
            cJSON_Delete(root); ok=id[0]!=0;
        }
        TickType_t start=xTaskGetTickCount();
        while(ok) {
            taskENTER_CRITICAL(); int done=complete; taskEXIT_CRITICAL();
            if(done) break;
            if(xTaskGetTickCount()-start>pdMS_TO_TICKS(31000)) { ok=0; break; }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        taskENTER_CRITICAL(); recording=0; unsigned count=used,voice=voiced; taskEXIT_CRITICAL();
        if(ok && voice>=3200) {
            snprintf(path,sizeof(path),"/voice/%s/audio",id);
            ok=!post(path,speech,count*2,response,sizeof(response));
            printf("[voice] %s\n",ok?"submitted":"submission failed; no automatic retry");
        } else {
            if(id[0]) { snprintf(path,sizeof(path),"/voice/%s/cancel",id); post(path,NULL,0,response,sizeof(response)); }
            printf("[voice] cancelled: no speech or bridge unavailable\n");
        }
        /* Restore keyword detection after every capture, including failures. */
        acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);
        taskENTER_CRITICAL(); pending=0; taskEXIT_CRITICAL();
    }
}
int pet_voice_init(void) {
    speech=lisa_mem_alloc(MAX_SAMPLES*2);
    if(!speech) return -1;
    ic_message_init();
    if(acomp_init() || acomp_wakeup_init() || acomp_wakeup_prepare()) return -1;
    acomp_stream_chn_create_desc_t tx={.cname="stream.mix2ch",.direction=ACOMP_STREAM_DIRECTION_M2R,
        .index=0,.buffer_size=ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONCE_FRAME,.num_descs=4,.kick_policy=1};
    acomp_stream_chn_create_desc_t rx={.cname="stream.tocloud",.direction=ACOMP_STREAM_DIRECTION_R2M,
        .index=1,.buffer_size=ACOMP_WAKEUP_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME,.num_descs=8,.kick_policy=1};
    if(acomp_wakeup_stream_ch_enable(0,&tx) || acomp_wakeup_stream_ch_enable(1,&rx)) return -1;
    if(xTaskCreate(upload,"voice-upload",4096,NULL,2,&upload_task)!=pdPASS ||
       xTaskCreate(drain,"voice-drain",2048,NULL,3,NULL)!=pdPASS) return -1;
    if(acomp_wakeup_add_callback(WAKEUP_CB_EVENT_ENGINE_RLT,wake_event,NULL) ||
       acomp_wakeup_start() || acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP)) return -1;
    lisa_device_t *device=lisa_device_get("audio0");
    lisa_audio_record_config_t cfg={.format={.sample_rate=LISA_AUDIO_RATE_16K,
        .channels=LISA_AUDIO_CH_STEREO,.sample_bits=LISA_AUDIO_BIT_16},
        .gain={.analog_gain=30,.digital_gain=0},.differential_input=true,.enable_hpf=false};
    if(!device || lisa_audio_record_config(device,&cfg) ||
       lisa_audio_register_callback(device,audio_event,NULL) || lisa_audio_record_start(device)) return -1;
    printf("[voice] wake word ready\n");
    return 0;
}
