#ifndef PET_NET_H
#define PET_NET_H
#ifdef LISTENM_HOST
#include <netinet/in.h>
#else
#include <lwip/inet.h>
#endif
/* Resolve on every new connection; lwIP manages DNS/mDNS cache expiry. */
int pet_resolve_ipv4(const char *host, struct in_addr *address);
#endif
