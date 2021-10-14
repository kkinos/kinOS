#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../libs/gui/guisyscall.hpp"

static const int kWidth = 200, kHeight = 130;

bool IsInside(int x, int y) {
    return 4 <= x && x < 4 + kWidth && 24 <= y && y < 24 + kHeight;
}

extern "C" void main(int argc, char** argv) {
    int layer_id = OpenWindow(kWidth + 8, kHeight + 28, 10, 10);
    if (layer_id == -1) {
        exit(1);
    }

    Message msg[1];
    bool press = false;
    while (true) {
        SyscallOpenReceiveMessage(msg, 1);
        if (msg[0].type == Message::kQuit) {
            break;
        } else if (msg[0].type == Message::kMouseMove) {
            auto& arg = msg[0].arg.mouse_move;
            const auto prev_x = arg.x - arg.dx, prev_y = arg.y - arg.dy;
            if (press && IsInside(prev_x, prev_y) && IsInside(arg.x, arg.y)) {
                WinDrawLine(layer_id, true, prev_x, prev_y, arg.x, arg.y,
                            0x000000);
            }
        } else if (msg[0].type == Message::kMouseButton) {
            auto& arg = msg[0].arg.mouse_button;
            if (arg.button == 0) {
                press = arg.press;
                WinFillRectangle(layer_id, true, arg.x, arg.y, 1, 1, 0x000000);
            }
        } else {
            printf("unknown event: type = %d\n", msg[0].type);
        }
    }

    CloseWindow(layer_id);
    exit(0);
}