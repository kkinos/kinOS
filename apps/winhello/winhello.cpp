#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../libs/kinos/app/gui/guisyscall.hpp"

extern "C" void main(int argc, char** argv) {
    int layer_id = OpenWindow(200, 100, 10, 10, "winhello");
    if (layer_id == -1) {
        exit(1);
    }

    WinWriteString(layer_id, true, 7, 24, 0xc00000, "hello world!");
    WinWriteString(layer_id, true, 24, 40, 0x00c000, "hello world!");
    WinWriteString(layer_id, true, 40, 56, 0x0000c0, "hello world!");

    Message msg[1];
    while (true) {
        SyscallOpenReceiveMessage(msg, 1);
        if (msg[0].type == Message::kQuit) {
            break;
        } else {
            printf("unknown event: type = %d\n", msg[0].type);
        }
    }
    CloseWindow(layer_id);
    exit(0);
}