#define _POSIX_C_SOURCE 200809L
#include "pet_overlay.h"
#include "pet_model.h"
#include "whale.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void write_file(const char *name,const void *data,size_t n) {
    FILE *f=fopen(name,"wb"); assert(f); assert(fwrite(data,1,n,f)==n); assert(!fclose(f));
}
int main(int argc,char **argv) {
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb"); assert(f);
    assert(!fseek(f,0,SEEK_END)); size_t n=(size_t)ftell(f); rewind(f);
    uint8_t *blob=malloc(n); assert(blob); assert(fread(blob,1,n,f)==n); fclose(f);
    char dir[]="/tmp/listenm-overlay-XXXXXX"; assert(mkdtemp(dir)); assert(!chdir(dir));
    uint16_t pixels[208*196],expected[208*196];
    uint16_t background[240*240]; memset(background,0x35,sizeof(background));
    pet_overlay_init(n,0); assert(pet_overlay_background(background)==-1); pet_overlay_close();
    write_file("background.rgb565",background,sizeof(background));
    write_file("whale.bin",blob,n);
    write_file("click.bin",blob,n);
    pet_overlay_init(n,n);
    memset(background,0,sizeof(background)); assert(!pet_overlay_background(background));
    for(unsigned i=0;i<240*240;i++) assert(background[i]==0x3535);
    /* Use a different valid frame at the call site to prove the file is read. */
    memset(pixels,0,sizeof(pixels)); memset(expected,0,sizeof(expected));
    unsigned begin=whale_clips[0].frames[1],end=whale_clips[0].frames[2];
    assert(!pet_frame_decode(blob+begin,end-begin,blob,expected,208*196));
    assert(!pet_overlay_frame(0,0,begin,end,pixels)); assert(!memcmp(pixels,expected,sizeof(pixels)));
    memset(pixels,0,sizeof(pixels));
    assert(!pet_overlay_frame(1,0,begin,end,pixels)); assert(!memcmp(pixels,expected,sizeof(pixels)));
    assert(pet_overlay_frame(1,0,begin,n+1,pixels)==-1);
    assert(!memcmp(pixels,expected,sizeof(pixels)));
    pet_overlay_close();
    /* Malformed frame must not partially alter the destination. */
    memset(blob+begin,255,end-begin); write_file("whale.bin",blob,n);
    pet_overlay_init(n,0); memset(pixels,0x23,sizeof(pixels)); memcpy(expected,pixels,sizeof(pixels));
    assert(pet_overlay_frame(0,0,begin,end,pixels)==-1); assert(!memcmp(pixels,expected,sizeof(pixels)));
    pet_overlay_close();
    write_file("whale.bin",blob,n-1); write_file("background.rgb565",blob,1);
    pet_overlay_init(n,0); assert(pet_overlay_background(background)==-1);
    assert(pet_overlay_frame(0,0,begin,end,pixels)==-1); pet_overlay_close();
    unlink("click.bin"); unlink("whale.bin"); unlink("background.rgb565"); assert(!chdir("/")); assert(!rmdir(dir));
    free(blob); puts("Overlay priority, missing/truncated assets and atomic corruption fallback passed");
    return 0;
}
