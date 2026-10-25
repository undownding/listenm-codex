#include "pet_overlay.h"
#include "pet_model.h"
#include "pet_pack.h"
#include <stdio.h>
#include <string.h>
#ifdef LISTENM_HOST
#include <stdlib.h>
#define lisa_mem_alloc malloc
#define lisa_mem_free free
static FILE *files[4];
static int open_file(unsigned i,const char *name,size_t size) {
    files[i]=fopen(name,"rb");
    if(!files[i]) return -1;
    if(fseek(files[i],0,SEEK_END) || (size && ftell(files[i])!=(long)size)) return -1;
    return 0;
}
static size_t file_length(unsigned i) { return (size_t)ftell(files[i]); }
static void close_file(unsigned i) { if(files[i]) fclose(files[i]); files[i]=NULL; }
static int read_at(unsigned i,size_t offset,void *buf,size_t size) {
    return !files[i] || fseek(files[i],(long)offset,SEEK_SET) || fread(buf,1,size,files[i])!=size ? -1:0;
}
#else
#include <lisa_mem.h>
#include <lsfs.h>
static struct lsfs_file_t files[4];
static int opened[4];
static int open_file(unsigned i,const char *name,size_t size) {
    char path[160]; snprintf(path,sizeof(path),"/SD:/%s",name);
    lsfs_file_t_init(&files[i]);
    if(lsfs_open(&files[i],path,LSFS_O_READ)) return -1;
    opened[i]=1;
    return !size || lsfs_lsize(&files[i])==(off_t)size?0:-1;
}
static size_t file_length(unsigned i) { return (size_t)lsfs_lsize(&files[i]); }
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
static size_t lengths[4];
static int active[4];
void pet_overlay_close(void) {
    for(unsigned i=0;i<4;i++) { close_file(i); active[i]=0; }
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
static int decode_file(unsigned i,size_t palette,size_t begin,size_t end,uint16_t *pixels) {
    if(!active[i]) return -1;
    if(begin>=end || end>lengths[i] || end-begin>FRAME_LIMIT ||
       palette>lengths[i] || lengths[i]-palette<512 ||
       read_at(i,palette,palette_data,512) || read_at(i,begin,frame,end-begin)) goto fail;
    memcpy(scratch,pixels,PIXELS*2);
    if(pet_frame_decode(frame,end-begin,palette_data,scratch,PIXELS)) goto fail;
    memcpy(pixels,scratch,PIXELS*2);
    return 0;
fail:
    printf("[overlay] %s invalid/read failed; stopping animation\n",i==3?"extra":i==2?"click.bin":"whale.bin");
    close_file(i); active[i]=0;
    return -1;
}

int pet_overlay_read_click(size_t offset,void *data,size_t size) {
    return !active[2] || offset>lengths[2] || size>lengths[2]-offset ? -1:read_at(2,offset,data,size);
}

int pet_overlay_frame(int click,size_t palette,size_t begin,size_t end,uint16_t *pixels) {
    return decode_file(click?2:1,palette,begin,end,pixels);
}

static uint32_t extra_offsets[601];
static size_t extra_base;
static unsigned extra_count;
void pet_overlay_extra_close(void) {
    close_file(3); active[3]=0; extra_count=0;
}
unsigned pet_overlay_extra_open(const char *name) {
    pet_overlay_extra_close();
    if(!name || !*name) return 0;
    for(const char *p=name;*p;p++)
        if(!((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_')) return 0;
    char path[128];
    if(snprintf(path,sizeof(path),"extras/%s.bin",name)>=(int)sizeof(path)) return 0;
    const char *stage="open";
    if(open_file(3,path,0)) goto invalid;
    stage="header";
    lengths[3]=file_length(3);
    uint8_t header[16], offset[4];
    if(lengths[3]<532 || lengths[3]>1750000 || read_at(3,0,header,16) ||
       memcmp(header,"WHP2",4) || pet_u32(header+8)!=208 || pet_u32(header+12)!=196) goto invalid;
    extra_count=pet_u32(header+4);
    if(!extra_count || extra_count>600) goto invalid;
    extra_base=16+4*(extra_count+1);
    if(extra_base+512>lengths[3]) goto invalid;
    stage="frame index";
    for(unsigned i=0;i<=extra_count;i++) {
        if(read_at(3,16+4*i,offset,4)) goto invalid;
        extra_offsets[i]=pet_u32(offset);
        if(extra_offsets[i]>lengths[3]-extra_base ||
           (!i && extra_offsets[i]!=512) ||
           (i && (extra_offsets[i]<=extra_offsets[i-1] ||
                  extra_offsets[i]-extra_offsets[i-1]>FRAME_LIMIT))) goto invalid;
    }
    if(extra_offsets[extra_count]!=lengths[3]-extra_base || !frame || !scratch) goto invalid;
    active[3]=1;
    printf("[pet-ui] extra TF=%s bytes=%u\n",name,(unsigned)lengths[3]);
    return extra_count;
invalid:
    printf("[pet-extra] TF load failed path=%s stage=%s\n",path,stage);
    pet_overlay_extra_close(); return 0;
}
int pet_overlay_extra_frame(unsigned index,uint16_t *pixels) {
    if(index>=extra_count) return -1;
    return decode_file(3,extra_base,extra_base+extra_offsets[index],extra_base+extra_offsets[index+1],pixels);
}
