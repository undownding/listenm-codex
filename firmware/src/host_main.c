#include "receiver.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    signal(SIGPIPE, SIG_IGN);
    char cursor[96] = {0};
    for (;;) {
        receiver_run(argv[1], argc > 2 ? (unsigned)atoi(argv[2]) : 8787,
            getenv("BRIDGE_TOKEN") ? getenv("BRIDGE_TOKEN") : "", cursor);
        sleep(2);
    }
}
