/**
 * @file mikanos.cpp
 *
 * MikanOS版のOSサーバー
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "mouse.hpp"

extern "C" int main() {
    DrawDesktop();
    InitializeConsole();
    InitializeMouse();
    
    console->PutString("unko\n");
    console->PutString("chinchin\n");
    console->PutString("chinchin\n");

    AppMessage msg[1]; 

    while (true) {
    auto [ n, err ] = SyscallReceiveMessage(msg, 1);
    if (err) {
      printf("Receive message failed: %s\n", strerror(err));
      break;
    }
    if (msg[0].type == AppMessage::aMouseMove) {
        auto& arg = msg[0].arg.mouse_move;
        mouse_cursor->MoveRelative({ arg.dx, arg.dy });
    }
  }

    exit(0);

    
}