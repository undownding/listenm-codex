#include "pet_extra.h"
#include "pet_config.h"
#include <FreeRTOS.h>
#include <task.h>
#include <lisa_mem.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LIMIT 1750000
static int enabled;
static unsigned generation;
static uint8_t *pending;
static size_t pending_size;
uint8_t *pet_extra_take(int allow,size_t *size) {
    taskENTER_CRITICAL();
    if(enabled!=allow) { enabled=allow; generation++; }
    uint8_t *p=pending; *size=pending_size; pending=NULL;
    taskEXIT_CRITICAL();
    if(!allow && p) { lisa_mem_free(p); p=NULL; }
    return p;
}
static uint8_t *fetch(size_t *length) {
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return NULL;
    uint8_t *data=NULL;
    struct timeval timeout={.tv_sec=5};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(PET_BRIDGE_PORT)};
    inet_pton(AF_INET,PET_BRIDGE_IP,&addr.sin_addr);
    if(connect(fd,(struct sockaddr*)&addr,sizeof(addr))) goto done;
    char header[1024];
    int n=snprintf(header,sizeof(header),"GET /pet/next HTTP/1.0\r\nHost: %s\r\nAuthorization: Bearer %s\r\nConnection: close\r\n\r\n",PET_BRIDGE_IP,PET_BRIDGE_TOKEN);
    if(n<0 || n>=(int)sizeof(header)) goto done;
    for(int sent=0;sent<n;) { int k=send(fd,header+sent,n-sent,0); if(k<=0) goto done; sent+=k; }
    size_t used=0;
    while(used<sizeof(header)-1) {
        if(recv(fd,header+used,1,0)!=1) goto done;
        header[++used]=0;
        if(used>=4 && !memcmp(header+used-4,"\r\n\r\n",4)) break;
    }
    int status=0; sscanf(header,"HTTP/%*s %d",&status);
    if(status!=200) goto done;
    for(size_t i=0;i<used;i++) if(header[i]>='A' && header[i]<='Z') header[i]+='a'-'A';
    char *len=strstr(header,"\r\ncontent-length: ");
    if(!len || strstr(header,"transfer-encoding:")) goto done;
    unsigned long size=strtoul(len+18,NULL,10);
    if(size<532 || size>LIMIT) goto done;
    data=lisa_mem_alloc(size); if(!data) goto done;
    for(used=0;used<size;) {
        n=recv(fd,data+used,size-used,0);
        if(n<=0) { lisa_mem_free(data); data=NULL; goto done; }
        used+=n;
    }
    *length=size;
    char *name=strstr(header,"\r\nx-animation: ");
    if(name) { name+=15; char *end=strchr(name,'\r'); if(end) *end=0; printf("[pet-ui] extra downloaded=%s bytes=%lu\n",name,size); }
done:
    closesocket(fd); return data;
}
static void worker(void *arg) {
    (void)arg; unsigned idle=0;
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        taskENTER_CRITICAL(); int allow=enabled && !pending; unsigned gen=generation; taskEXIT_CRITICAL();
        if(!allow) { idle=0; continue; }
        if(++idle<20) continue;
        idle=0; size_t size=0; uint8_t *data=fetch(&size);
        taskENTER_CRITICAL();
        if(data && enabled && generation==gen && !pending) { pending=data; pending_size=size; data=NULL; }
        taskEXIT_CRITICAL();
        if(data) lisa_mem_free(data);
    }
}
int pet_extra_init(void) { return xTaskCreate(worker,"pet-extra",4096,NULL,tskIDLE_PRIORITY+1,NULL)==pdPASS?0:-1; }
