#include "sse.h"
#include <string.h>
void sse_init(sse_parser *p, sse_callback cb, void *ctx) {
    memset(p, 0, sizeof(*p)); p->callback = cb; p->ctx = ctx;
}
static int line(sse_parser *p) {
    p->line[p->line_len] = 0;
    if (!p->line_len) {
        if (p->data_len) {
            p->data[--p->data_len] = 0;
            p->callback(p->event[0] ? p->event : "message", p->id, p->data, p->ctx);
        }
        p->data_len = 0; p->data[0] = 0; p->event[0] = 0;
        return 0;
    }
    char *value = strchr(p->line, ':');
    if (value) { *value++ = 0; if (*value == ' ') value++; }
    else value = p->line + p->line_len;
    size_t n = strlen(value);
    if (!strcmp(p->line, "data")) {
        if (p->data_len + n + 1 >= sizeof(p->data)) return -1;
        memcpy(p->data + p->data_len, value, n); p->data_len += n;
        p->data[p->data_len++] = '\n';
    } else if (!strcmp(p->line, "event")) {
        if (n >= sizeof(p->event)) return -1;
        memcpy(p->event, value, n + 1);
    } else if (!strcmp(p->line, "id")) {
        if (n >= sizeof(p->id)) return -1;
        memcpy(p->id, value, n + 1);
    }
    return 0;
}
int sse_feed(sse_parser *p, const char *bytes, size_t size) {
    if (p->failed) return -1;
    for (size_t i = 0; i < size; i++) {
        char c = bytes[i];
        if (c == '\n' && p->cr) { p->cr = 0; continue; }
        p->cr = c == '\r';
        if (c == '\n' || c == '\r') {
            if (line(p)) { p->failed = 1; return -1; }
            p->line_len = 0;
        } else {
            if (!c || p->line_len + 1 >= sizeof(p->line)) { p->failed = 1; return -1; }
            p->line[p->line_len++] = c;
        }
    }
    return 0;
}
