#include "pet_storage.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef LISTENM_HOST
#include <stdlib.h>
#define lisa_mem_alloc malloc
#define lisa_mem_free free
int pet_storage_init(void) { return 0; }
#else
#include <lisa_mem.h>
#include <lisa_device.h>
#include <lisa_gpio.h>
#include <lisa_sdmmc.h>
#include <board.h>
#include <lsfs.h>
#include <disk/disk_access.h>
static struct lsfs_mount_t mount={.type=LSFS_FATFS,.mnt_point="/SD:"};
int pet_storage_init(void) {
#ifdef TF_DET_DEVICE_NAME
    lisa_device_t *det=lisa_device_get(TF_DET_DEVICE_NAME);
    if(!det || lisa_gpio_configure(det,TF_DET_PIN,LISA_GPIO_INPUT) ||
       lisa_gpio_read_pin(det,TF_DET_PIN)!=TF_DET_ACTIVE_LEVEL) return -1;
#endif
    disk_init(NULL); lsfs_init();
    lisa_device_t *sd=lisa_device_get("sdmmc0");
    return !sd || lisa_sdmmc_probe(sd) || lsfs_mount(&mount) ? -1:0;
}
#endif
void *pet_storage_load(const char *name,size_t limit,size_t *size) {
    *size=0;
    if(!name || !*name || name[0]=='/' || strstr(name,"..") || strchr(name,':') || strchr(name,'\\')) return NULL;
    uint8_t *data=NULL;
#ifdef LISTENM_HOST
    FILE *f=fopen(name,"rb"); if(!f) return NULL;
    if(fseek(f,0,SEEK_END)) goto done;
    long length=ftell(f); if(length<=0 || (unsigned long)length>limit) goto done;
    rewind(f); data=lisa_mem_alloc((size_t)length+1);
    if(!data) goto done;
    if(fread(data,1,(size_t)length,f)!=(size_t)length) { lisa_mem_free(data); data=NULL; goto done; }
#else
    char path[160]; if(snprintf(path,sizeof(path),"/SD:/%s",name)>=(int)sizeof(path)) return NULL;
    struct lsfs_file_t file; lsfs_file_t_init(&file);
    if(lsfs_open(&file,path,LSFS_O_READ)) return NULL;
    off_t length=lsfs_lsize(&file); if(length<=0 || (size_t)length>limit) goto done;
    data=lisa_mem_alloc((size_t)length+1); if(!data) goto done;
    size_t used=0;
    while(used<(size_t)length) {
        ssize_t n=lsfs_read(&file,data+used,(size_t)length-used);
        if(n<=0) { lisa_mem_free(data); data=NULL; goto done; } used+=(size_t)n;
    }
#endif
    data[length]=0; *size=(size_t)length;
done:
#ifdef LISTENM_HOST
    fclose(f);
#else
    lsfs_close(&file);
#endif
    return data;
}
