#ifndef LISTENM_SSE_H
#define LISTENM_SSE_H
#include <stddef.h>
typedef void (*sse_callback)(const char *event, const char *id, const char *data, void *ctx);
typedef struct {
    char line[1024], data[2048], event[64], id[96];
    size_t line_len, data_len;
    int failed, cr;
    sse_callback callback;
    void *ctx;
} sse_parser;
void sse_init(sse_parser *p, sse_callback cb, void *ctx);
int sse_feed(sse_parser *p, const char *bytes, size_t size);
#endif
