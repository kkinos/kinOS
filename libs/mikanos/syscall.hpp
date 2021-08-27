#pragma once

#include "../../kernel/message.hpp"
#include "../kinos/syscall.h"

void OpenWindow(int w, int h, int x, int y) {
    Message msg{Message::aOpenWindow};
    msg.arg.openwindow.w = w;
    msg.arg.openwindow.h = h;
    msg.arg.openwindow.x = x;
    msg.arg.openwindow.y = y;

    SyscallSendMessageToOs(&msg);
}