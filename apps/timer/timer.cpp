#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../libs/mikanos/mikansyscall.hpp"

extern "C" void main(int argc, char** argv) {
    if (argc <= 1) {
        printf("Usage: timer <msec>\n");
        exit(1);
    }

    const unsigned long duration_ms = atoi(argv[1]);
    const auto timeout = SyscallCreateTimer(TIMER_ONESHOT_REL, 1, duration_ms);
    printf("timer created. timeout = %lu\n", timeout.value);

    Message msg[1];
    while (true) {
        SyscallOpenReceiveMessage(msg, 1);
        if (msg[0].type == Message::kTimerTimeout) {
            printf("%lu msecs elapsed!\n", duration_ms);
            break;
        } else {
            printf("unknown event: type = %d\n", msg[0].type);
        }
    }
    exit(0);
}