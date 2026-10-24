#define _POSIX_C_SOURCE 200809L
#include "pet_config.h"
#include "pet_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char valid[]="bridge_ip=192.168.1.2\nbridge_token=secret # literal\nwifi_ssid=My WiFi\nwifi_password=\nwake_word=xiao ling xiao ling\nwake_model=voice/algo.bin\nwake_wrap=voice/wrap.json\n";
int main(void) {
    pet_config c={0}; assert(!pet_config_parse(valid,strlen(valid),&c));
    assert(c.bridge_port==8787 && c.voice_threshold==600 && !strcmp(c.bridge_token,"secret # literal"));
    assert(!strcmp(c.wake_word,"xiao ling xiao ling") && !c.wifi_password[0]);
    const char *bad[]={"bridge_port=0","bridge_port=65536","voice_threshold=32768","bridge_port=8x","bridge_ip=256.1.1.1","unknown=yes","bridge_token=x","wake_word=x","wifi_ssid=x","broken","bridge_port=9999999999999999999999999"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        char buf[4096]; snprintf(buf,sizeof(buf),"%s%s\n",valid,bad[i]);
        pet_config old=c; assert(pet_config_parse(buf,strlen(buf),&c)==-1); assert(!memcmp(&old,&c,sizeof(c)));
    }
    const char *ips[]={"256.1.1.1","1.2.3","1.2.3.4.5","1.2.3.4x","01.2.3.4","+1.2.3.4","1. 2.3.4"};
    for(unsigned i=0;i<sizeof(ips)/sizeof(ips[0]);i++) {
        char buf[512]; snprintf(buf,sizeof(buf),"bridge_ip=%s\nbridge_token=x\nwifi_ssid=x\n",ips[i]);
        assert(pet_config_parse(buf,strlen(buf),&c)==-1);
    }
    char bounds[512]; snprintf(bounds,sizeof(bounds),"%sbridge_port=65535\nvoice_threshold=32767\n",valid);
    assert(!pet_config_parse(bounds,strlen(bounds),&c) && c.bridge_port==65535 && c.voice_threshold==32767);
    assert(pet_config_parse(NULL,0,&c)==-1);
    assert(pet_config_parse("a\0b",3,&c)==-1);
    assert(pet_config_parse("wifi_ssid=x",11,&c)==-1);
    char dir[]="/tmp/listenm-config-XXXXXX"; assert(mkdtemp(dir)); assert(!chdir(dir));
    assert(pet_config_load()==-1);
    FILE *f=fopen("config.ini","wb"); assert(f); assert(fwrite(valid,1,strlen(valid),f)==strlen(valid)); fclose(f);
    assert(!pet_storage_init() && !pet_config_load());
    size_t n; assert(!pet_storage_load("config.ini",2,&n) && !n);
    assert(!pet_storage_load("../config.ini",4096,&n));
    char *data=pet_storage_load("config.ini",4096,&n); assert(data && n==strlen(valid) && !data[n]); free(data);
    unlink("config.ini"); assert(!chdir("/")); assert(!rmdir(dir));
    puts("Runtime configuration validation and bounded TF reads passed");
}
