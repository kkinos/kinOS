#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "../../libs/mikanos/mikansyscall.hpp"

static const int kCanvasSize = 100, kEyeSize = 10;

void DrawEye(uint64_t layer_id_flags, int mouse_x, int mouse_y,
             uint32_t color) {
    const double center_x = mouse_x - kCanvasSize / 2 - 4;
    const double center_y = mouse_y - kCanvasSize / 2 - 24;

    const double direction = atan2(center_y, center_x);
    double distance = sqrt(pow(center_x, 2) + pow(center_y, 2));
    distance = std::min<double>(distance, kCanvasSize / 2 - kEyeSize / 2);

    const double eye_center_x = cos(direction) * distance;
    const double eye_center_y = sin(direction) * distance;
    const int eye_x = static_cast<int>(eye_center_x) + kCanvasSize / 2 + 4;
    const int eye_y = static_cast<int>(eye_center_y) + kCanvasSize / 2 + 24;

    WinFillRectangle(layer_id_flags, true, eye_x - kEyeSize / 2,
                     eye_y - kEyeSize / 2, kEyeSize, kEyeSize, color);
}

extern "C" void main(int argc, char** argv) {
    int layer_id = OpenWindow(kCanvasSize + 8, kCanvasSize + 28, 10, 10);
    if (layer_id == -1) {
        exit(1);
    }

    WinFillRectangle(layer_id, true, 4, 24, kCanvasSize, kCanvasSize, 0xffffff);

    Message msg[1];
    while (true) {
        SyscallOpenReceiveMessage(msg, 1);
        if (msg[0].type == Message::aQuit) {
            break;
        } else if (msg[0].type == Message::aMouseMove) {
            auto& arg = msg[0].arg.mouse_move;
            WinFillRectangle(layer_id, false, 4, 24, kCanvasSize, kCanvasSize,
                             0xffffff);
            DrawEye(layer_id, arg.x, arg.y, 0x000000);
        } else {
            printf("unknown event: type = %d\n", msg[0].type);
        }
    }
    CloseWindow(layer_id);
    exit(0);
}