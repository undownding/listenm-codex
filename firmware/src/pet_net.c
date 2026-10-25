#define _POSIX_C_SOURCE 200809L
#include "pet_net.h"
#include <string.h>
#include <stdio.h>
#ifdef LISTENM_HOST
#include <arpa/inet.h>
#include <netdb.h>
#else
#include <lwip/netdb.h>
#endif
static int resolve_name(const char *host, struct in_addr *address) {
    struct addrinfo hints={0}, *result=NULL;
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    int error=getaddrinfo(host,NULL,&hints,&result);
    if(error || !result) {
        printf("[pet-net] cannot resolve %s (%d)\n",host,error);
        if(result) freeaddrinfo(result);
        return -1;
    }
    int found=0;
    for(struct addrinfo *p=result;p;p=p->ai_next) {
        if(p->ai_family==AF_INET && p->ai_addr && p->ai_addrlen>=sizeof(struct sockaddr_in)) {
            struct in_addr candidate=((struct sockaddr_in *)p->ai_addr)->sin_addr;
            uint32_t first=ntohl(candidate.s_addr)>>24;
            /* A host resolver can expose its own /etc/hosts loopback entry.
             * Such an address cannot identify the remote bridge device. */
            if(!first || first==127 || first>=224) continue;
            *address=candidate;
            found=1; break;
        }
    }
    freeaddrinfo(result);
    if(found) {
        char ip[16];
        if(inet_ntop(AF_INET,address,ip,sizeof(ip))) printf("[pet-net] %s -> %s\n",host,ip);
    }
    else printf("[pet-net] no usable remote IPv4 for %s\n",host);
    return found?0:-1;
}

int pet_resolve_ipv4(const char *host, struct in_addr *address) {
    if(!host || !*host || !address) return -1;
    if(inet_pton(AF_INET,host,address)==1) return 0;
    if(!resolve_name(host,address)) return 0;
    /* Short names use the network DNS first (including a configured tailnet).
     * Only unresolved short names fall back to link-local multicast DNS. */
    if(!strchr(host,'.') && strlen(host)<=63) {
        char local[70];
        snprintf(local,sizeof(local),"%s.local",host);
        return resolve_name(local,address);
    }
    return -1;
}
