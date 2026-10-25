#include "pet_model.h"
#include <string.h>
int pet_state_parse(const char *name, pet_state *out) {
    static const char *names[]={"idle","thinking","working","speaking","attention","sleeping","error"};
    if (!name || !out) return -1;
    for (unsigned i=0;i<PET_STATE_COUNT;i++) if (!strcmp(name,names[i])) { *out=(pet_state)i; return 0; }
    return -1;
}
void pet_model_event(pet_model *m, pet_state state) { if (state<PET_STATE_COUNT) m->requested=state; }
int pet_model_tick(pet_model *m, uint32_t now) {
    pet_state next=m->connected ? m->requested : PET_IDLE;
    if (next==m->current) return 0;
    if (m->connected && next!=PET_ERROR && next!=PET_ATTENTION && (uint32_t)(now-m->changed)<700) return 0;
    m->current=next; m->changed=now; return 1;
}
typedef struct { const uint8_t *src; size_t size,pos; unsigned left; int repeat; } rle_reader;
static int next_byte(rle_reader *r,unsigned *value) {
    if(!r->left) {
        if(r->pos>=r->size) return -1;
        unsigned cmd=r->src[r->pos++]; r->repeat=cmd>=128;
        r->left=r->repeat?cmd-127:cmd+1;
        if((r->repeat?1:r->left)>r->size-r->pos) return -1;
    }
    *value=r->src[r->pos];
    if(!r->repeat || r->left==1) r->pos++;
    r->left--; return 0;
}
int pet_frame_decode_scaled(const uint8_t *src, size_t size, const uint8_t *palette,
                            uint16_t *dst, size_t width, size_t height,
                            size_t out_width, size_t out_height, size_t stride) {
    if(!width || !height || !out_width || !out_height || stride<out_width) return -1;
    if(size<4) return -1;
    uint32_t colors=(uint32_t)src[0]|((uint32_t)src[1]<<8)|((uint32_t)src[2]<<16)|((uint32_t)src[3]<<24);
    if(colors>size-4) return -1;
    rle_reader rgb={.src=src+4,.size=colors},alpha={.src=src+4+colors,.size=size-4-colors};
    for(size_t i=0;i<width*height;i++) {
        unsigned idx,a;
        if(next_byte(&rgb,&idx) || next_byte(&alpha,&a)) return -1;
        if(!a) continue;
        unsigned fg=palette[idx*2]|((unsigned)palette[idx*2+1]<<8);
        /* Expand before alpha blending so the backdrop stays at native resolution. */
        size_t x=i%width,y=i/width;
        size_t x0=(x*out_width+width-1)/width;
        size_t x1=((x+1)*out_width+width-1)/width;
        size_t y0=(y*out_height+height-1)/height;
        size_t y1=((y+1)*out_height+height-1)/height;
        for(size_t oy=y0;oy<y1;oy++) for(size_t ox=x0;ox<x1;ox++) {
            size_t pos=oy*stride+ox;
            if(a==255) { dst[pos]=(uint16_t)fg; continue; }
            unsigned bg=dst[pos],inv=255-a;
            unsigned r=(((fg>>11)*a+(bg>>11)*inv)+127)/255;
            unsigned g=((((fg>>5)&63)*a+((bg>>5)&63)*inv)+127)/255;
            unsigned b=(((fg&31)*a+(bg&31)*inv)+127)/255;
            dst[pos]=(uint16_t)((r<<11)|(g<<5)|b);
        }
    }
    return rgb.pos==rgb.size && !rgb.left && alpha.pos==alpha.size && !alpha.left?0:-1;
}

int pet_frame_decode(const uint8_t *src, size_t size, const uint8_t *palette, uint16_t *dst, size_t pixels) {
    return pet_frame_decode_scaled(src,size,palette,dst,pixels,1,pixels,1,pixels);
}
