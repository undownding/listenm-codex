#include "receiver.h"
#include "sse.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef LISTENM_HOST
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close
#else
#include "pet_ui.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#endif

static void received(const char *event, const char *id, const char *data, void *ctx) {
    char *last = ctx;
    if (!strcmp(event, "reset")) { last[0] = 0; printf("[pet] history reset\n"); return; }
    if (strcmp(event, "codex")) return;
#ifndef LISTENM_HOST
    pet_ui_event(data);
#endif
    printf("[pet] event=%s id=%s data=%s\n", event, id, data);
    fflush(stdout);
    memcpy(last, id, strlen(id) + 1);
}
int receiver_run(const char *host, unsigned port, const char *token, char last_id[96]) {
    int fd = socket(AF_INET, SOCK_STREAM, 0), result = -1;
    if (fd < 0) return -1;
    struct timeval timeout = {.tv_sec = 40, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) goto done;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) goto done;
    char request[768];
    if (strpbrk(token, "\r\n") || strpbrk(last_id, "\r\n")) goto done;
    int n = snprintf(request, sizeof(request),
        "GET /events HTTP/1.0\r\nHost: %s:%u\r\nAccept: text/event-stream\r\n"
        "Last-Event-ID: %s\r\n%s%s%sConnection: close\r\n\r\n",
        host, port, last_id, *token ? "Authorization: Bearer " : "", token, *token ? "\r\n" : "");
    if (n < 0 || n >= (int)sizeof(request)) goto done;
    for (int sent = 0; sent < n;) {
        int count = send(fd, request + sent, n - sent, 0);
        if (count <= 0) goto done;
        sent += count;
    }
    // Bounded header read supports arbitrary TCP fragmentation, and preserves
    // body bytes delivered with the final header. Bridge uses no chunked coding.
    char header[2048], buffer[512]; size_t used = 0; int ready = 0;
    sse_parser parser; sse_init(&parser, received, last_id);
    while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        int i = 0;
        if (!ready) {
            for (; i < n; i++) {
                if (used + 1 >= sizeof(header)) goto done;
                header[used++] = buffer[i]; header[used] = 0;
                if (used >= 4 && !memcmp(header + used - 4, "\r\n\r\n", 4)) {
                    int code = 0;
                    if (sscanf(header, "HTTP/%*s %d", &code) != 1 || code != 200) {
                        printf("[pet] HTTP status=%d\n", code); goto done;
                    }
                    // HTTP header field names are case insensitive.
                    for (size_t j = 0; j < used; j++)
                        if (header[j] >= 'A' && header[j] <= 'Z') header[j] += 'a' - 'A';
                    if (!strstr(header, "\r\ncontent-type: text/event-stream\r\n") ||
                        strstr(header, "\r\ntransfer-encoding:")) goto done;
                    ready = 1; i++;
#ifndef LISTENM_HOST
                    pet_ui_connected(1);
#endif
                    printf("[pet] SSE connected\n"); fflush(stdout); break;
                }
            }
        }
        if (ready && sse_feed(&parser, buffer + i, n - i)) {
            printf("[pet] invalid or oversized SSE frame\n"); goto done;
        }
    }
    result = ready ? 0 : -1;
done:
#ifndef LISTENM_HOST
    pet_ui_connected(0);
#endif
    closesocket(fd); return result;
}
