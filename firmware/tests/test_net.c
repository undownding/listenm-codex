#define _POSIX_C_SOURCE 200809L
#include "pet_net.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const char *queries[3];
static unsigned query_count, free_count;
static const char *accepted;
static int loopback_primary;
static struct sockaddr_in address;
static struct addrinfo answer;
int fixture_getaddrinfo(const char *name,const char *service,const struct addrinfo *hints,struct addrinfo **out) {
    (void)service;
    assert(hints->ai_family==AF_INET && hints->ai_socktype==SOCK_STREAM);
    assert(query_count<3 && !strcmp(name,queries[query_count++]));
    *out=NULL;
    int loopback=loopback_primary && !strcmp(name,queries[0]);
    if(!loopback && (!accepted || strcmp(name,accepted))) return EAI_NONAME;
    address.sin_family=AF_INET; assert(inet_pton(AF_INET,loopback?"127.0.1.1":"192.168.31.108",&address.sin_addr)==1);
    memset(&answer,0,sizeof(answer));answer.ai_family=AF_INET;
    answer.ai_addr=(struct sockaddr *)&address;answer.ai_addrlen=sizeof(address);
    *out=&answer;return 0;
}
void fixture_freeaddrinfo(struct addrinfo *value) { assert(value==&answer); free_count++; }
int main(void) {
    struct in_addr ip;
    assert(!pet_resolve_ipv4("192.168.31.108",&ip) && !query_count);
    assert(pet_resolve_ipv4(NULL,&ip)==-1);
    queries[0]="ubuntu-12700h";accepted=queries[0];
    assert(!pet_resolve_ipv4(queries[0],&ip) && query_count==1 && free_count==1);
    query_count=free_count=0;queries[1]="ubuntu-12700h.local";accepted=queries[1];
    assert(!pet_resolve_ipv4(queries[0],&ip) && query_count==2 && free_count==1);
    assert(!strcmp(inet_ntoa(ip),"192.168.31.108"));
    query_count=free_count=0;loopback_primary=1;
    assert(!pet_resolve_ipv4(queries[0],&ip) && query_count==2 && free_count==2);
    loopback_primary=0;
    query_count=free_count=0;accepted=NULL;
    assert(pet_resolve_ipv4(queries[0],&ip)==-1 && query_count==2 && !free_count);
    query_count=0;queries[0]="missing.example.com";
    assert(pet_resolve_ipv4(queries[0],&ip)==-1 && query_count==1);
    query_count=0;queries[0]="missing.local";
    assert(pet_resolve_ipv4(queries[0],&ip)==-1 && query_count==1);
    puts("IPv4, DNS priority, short-name mDNS fallback and resolution failure passed");
}
