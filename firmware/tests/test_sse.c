#include "sse.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int count;
static void event(const char *name, const char *id, const char *data, void *ctx) {
    (void)ctx;
    assert(!strcmp(name, "codex")); assert(!strcmp(id, "epoch:1"));
    assert(!strcmp(data, "first\nsecond")); count++;
}
int main(void) {
    const char *wire = ": heartbeat\r\nretry: 2000\r\n\r\nid: epoch:1\r\nevent: codex\r\ndata: first\r\ndata: second\r\n\r\n";
    for (size_t split = 0; split <= strlen(wire); split++) {
        sse_parser p; sse_init(&p, event, NULL); count = 0;
        assert(!sse_feed(&p, wire, split));
        assert(!sse_feed(&p, wire + split, strlen(wire) - split)); assert(count == 1);
    }
    sse_parser p; sse_init(&p, event, NULL); count = 0;
    for (size_t i = 0; i < strlen(wire); i++) assert(!sse_feed(&p, wire + i, 1));
    assert(count == 1);
    sse_init(&p, event, NULL);
    char huge[4096]; memset(huge, 'x', sizeof(huge));
    assert(sse_feed(&p, huge, sizeof(huge)) == -1);
    assert(sse_feed(&p, "\n\n", 2) == -1);
    puts("SSE fragmentation and bounds passed");
}
