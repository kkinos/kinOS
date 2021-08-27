#pragma once

#include "../common/message.hpp"
#include "../kinos/syscall.h"

int OpenWindow(int w, int h, int x, int y) {
    Message msg{Message::aOpenWindow};
    msg.arg.openwindow.w = w;
    msg.arg.openwindow.h = h;
    msg.arg.openwindow.x = x;
    msg.arg.openwindow.y = y;

    SyscallSendMessageToOs(&msg);

    Message rmsg[1];
    int layer_id;

    auto [ n, err ] = SyscallReceiveMessage(rmsg, 1);
    if (err) {
      return -1;
    }

    if (rmsg[0].type == Message::aLayerId) {
        layer_id = rmsg[0].arg.layerid.layerid;
    }

    return layer_id;
}

void WinFillRectangle(int layer_id, int x, int y, int w, int h, uint32_t color) {
    Message msg{Message::aWinFillRectangle};
    msg.arg.winfillrectangle.layer_id = layer_id;
    msg.arg.winfillrectangle.x = x;
    msg.arg.winfillrectangle.y = y;
    msg.arg.winfillrectangle.w = w;
    msg.arg.winfillrectangle.h = h;
    msg.arg.winfillrectangle.color = color;
    SyscallSendMessageToOs(&msg);

}