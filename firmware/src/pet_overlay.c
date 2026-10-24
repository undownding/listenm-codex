#include "pet_overlay.h"
#include "pet_model.h"
#include <stdio.h>
#include <string.h>
#ifdef LISTENM_HOST
#include <stdlib.h>
#define lisa_mem_alloc malloc
#define lisa_mem_free free
static FILE *files[3];
static int open_file(unsigned i,const char *name,size_t size) {
    files[i]=fopen(name,"rb");
    if(!files[i]) return -1;
    if(fseek(files[i],0,SEEK_END) || ftell(files[i])!=(long)size) return -1;
    return 0;
}
static void close_file(unsigned i) { if(files[i]) fclose(files[i]); files[i]=NULL; }
static int read_at(unsigned i,size_t offset,void *buf,size_t size) {
    return !files[i] || fseek(files[i],(long)offset,SEEK_SET) || fread(buf,1,size,files[i])!=size ? -1:0;
}
#else
#include <lisa_mem.h>
#include <lsfs.h>
static struct lsfs_file_t files[3];
static int opened[3];
static int open_file(unsigned i,const char *name,size_t size) {
    char path[64]; snprintf(path,sizeof(path),"/SD:/%s",name);
    lsfs_file_t_init(&files[i]);
    if(lsfs_open(&files[i],path,LSFS_O_READ)) return -1;
    opened[i]=1;
    return lsfs_lsize(&files[i])==(off_t)size?0:-1;
}
static void close_file(unsigned i) { if(opened[i]) lsfs_close(&files[i]); opened[i]=0; }
static int read_at(unsigned i,size_t offset,void *buf,size_t size) {
    if(!opened[i] || lsfs_seek(&files[i],offset,LSFS_SEEK_SET)) return -1;
    size_t used=0;
    while(used<size) {
        ssize_t n=lsfs_read(&files[i],(uint8_t *)buf+used,size-used);
        if(n<=0) return -1;
        used+=(size_t)n;
    }
    return 0;
}
#endif
#define PIXELS (208*196)
/* Two PackBits planes, including worst-case literal overhead and RGB length. */
#define FRAME_LIMIT (4+2*(PIXELS+(PIXELS+127)/128))
static uint8_t *frame;
static uint16_t *scratch;
static uint8_t palette_data[512];
static size_t lengths[3];
static int active[3];
void pet_overlay_close(void) {
    for(unsigned i=0;i<3;i++) { close_file(i); active[i]=0; }
    lisa_mem_free(frame); frame=NULL;
    lisa_mem_free(scratch); scratch=NULL;
}
void pet_overlay_init(size_t whale_size,size_t click_size) {
    const char *names[]={"background.rgb565","whale.bin","click.bin"};
    lengths[0]=240*240*2; lengths[1]=whale_size; lengths[2]=click_size;
    for(unsigned i=0;i<3;i++) {
        active[i]=!open_file(i,names[i],lengths[i]);
        if(!active[i]) close_file(i);
        else printf("[overlay] TF %s\n",names[i]);
    }
    if(active[1] || active[2]) {
        frame=lisa_mem_alloc(FRAME_LIMIT); scratch=lisa_mem_alloc(PIXELS*2);
        if(!frame || !scratch) {
            for(unsigned i=1;i<3;i++) { close_file(i); active[i]=0; }
            lisa_mem_free(frame); frame=NULL; lisa_mem_free(scratch); scratch=NULL;
        }
    }
}
int pet_overlay_background(uint16_t *pixels) {
    int result=active[0] && pixels?read_at(0,0,pixels,lengths[0]):-1;
    close_file(0); active[0]=0;
    return result;
}
int pet_overlay_frame(int click,size_t palette,size_t begin,size_t end,uint16_t *pixels) {
    unsigned i=click?2:1;
    if(!active[i]) return -1;
    if(begin>=end || end>lengths[i] || end-begin>FRAME_LIMIT ||
       palette>lengths[i] || lengths[i]-palette<512 ||
       read_at(i,palette,palette_data,512) || read_at(i,begin,frame,end-begin)) goto fail;
    memcpy(scratch,pixels,PIXELS*2);
    if(pet_frame_decode(frame,end-begin,palette_data,scratch,PIXELS)) goto fail;
    memcpy(pixels,scratch,PIXELS*2);
    return 0;
fail:
    printf("[overlay] %s invalid/read failed; stopping animation\n",click?"click.bin":"whale.bin");
    close_file(i); active[i]=0;
    return -1;
}

int pet_overlay_read_click(size_t offset,void *data,size_t size) {
    return !active[2] || offset>lengths[2] || size>lengths[2]-offset ? -1:read_at(2,offset,data,size);
}
