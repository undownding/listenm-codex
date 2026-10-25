#include "pet_config.h"
#include "pet_storage.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifndef LISTENM_HOST
#include <lisa_mem.h>
#else
#define lisa_mem_free free
#endif
pet_config pet_settings;
static char *trim(char *s) {
    while(*s==' ' || *s=='\t' || *s=='\r') s++;
    size_t n=strlen(s); while(n && (s[n-1]==' ' || s[n-1]=='\t' || s[n-1]=='\r')) s[--n]=0;
    return s;
}
int pet_config_parse(const char *data,size_t size,pet_config *out) {
    if(!data || !out || !size || size>4096 || memchr(data,0,size)) return -1;
    char text[4097]; memcpy(text,data,size); text[size]=0;
    pet_config c={.bridge_port=8787,.voice_threshold=600};
    struct field { const char *key; char *value; size_t cap; } fields[]={
#define FIELD(x) {#x,c.x,sizeof(c.x)}
        FIELD(bridge_ip),FIELD(bridge_token),FIELD(wifi_ssid),FIELD(wifi_password),
        FIELD(wake_word),FIELD(wake_model),FIELD(wake_wrap)
#undef FIELD
    };
    unsigned seen=0;
    for(char *line=text;line;) {
        char *next=strchr(line,'\n'); if(next) *next++=0;
        char *key=trim(line); line=next;
        if(!*key || *key=='#' || *key==';') continue;
        char *value=strchr(key,'='); if(!value) return -1;
        *value++=0; key=trim(key); value=trim(value);
        for(char *p=value;*p;p++) if((unsigned char)*p<32 || *p==127) return -1;
        unsigned i;
        for(i=0;i<7;i++) if(!strcmp(key,fields[i].key)) {
            if(strlen(value)>=fields[i].cap) return -1;
            strcpy(fields[i].value,value); break;
        }
        if(i==7) {
            if(!strcmp(key,"bridge_port")) i=7;
            else if(!strcmp(key,"voice_threshold")) i=8;
            else return -1;
            if(!*value) return -1;
            for(char *p=value;*p;p++) if(!isdigit((unsigned char)*p)) return -1;
            char *end; unsigned long n=strtoul(value,&end,10);
            if(*end || !n || n>(i==7?65535:32767)) return -1;
            if(i==7) c.bridge_port=(unsigned)n; else c.voice_threshold=(unsigned)n;
        }
        if(seen&(1u<<i)) return -1;
        seen|=1u<<i;
    }
    /* Keep bridge_ip as the backwards-compatible key, accepting DNS names too. */
    size_t length=strlen(c.bridge_ip);
    if(length && c.bridge_ip[length-1]=='.') c.bridge_ip[--length]=0;
    if(!length) return -1;
    int numeric=1;
    for(size_t i=0;i<length;i++) {
        unsigned char ch=(unsigned char)c.bridge_ip[i];
        if(ch>='A' && ch<='Z') c.bridge_ip[i]=(char)(ch-'A'+'a');
        if(!(ch>='0' && ch<='9') && ch!='.') numeric=0;
    }
    if(numeric) {
        const char *ip=c.bridge_ip;
        for(unsigned i=0;i<4;i++) {
            if(!isdigit((unsigned char)*ip) || (*ip=='0' && isdigit((unsigned char)ip[1]))) return -1;
            unsigned n=0;
            while(isdigit((unsigned char)*ip)) { n=n*10+(unsigned)(*ip++-'0'); if(n>255) return -1; }
            if(i<3) { if(*ip++!='.') return -1; } else if(*ip) return -1;
        }
    } else {
        size_t label=0;
        for(size_t i=0;i<=length;i++) {
            char ch=c.bridge_ip[i];
            if(!ch || ch=='.') {
                if(!label || c.bridge_ip[i-1]=='-') return -1;
                label=0;
            } else {
                if(!((ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='-') ||
                   (!label && ch=='-') || ++label>63) return -1;
            }
        }
    }
    if(!c.bridge_token[0] || !c.wifi_ssid[0] || !c.wake_word[0] || !c.wake_model[0] || !c.wake_wrap[0]) return -1;
    *out=c; return 0;
}
int pet_config_load(void) {
    size_t size; char *data=pet_storage_load("config.ini",4096,&size);
    int result=pet_config_parse(data,size,&pet_settings); lisa_mem_free(data); return result;
}
