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
#include "window.hpp"
#include "layer.hpp"
#include "shadow_buffer.hpp"

extern "C" int main() {
    InitializeGraphics();
    InitializeConsole();
    InitializeLayer();
    InitializeMouse();
    printk("\n");
    printk("###    ###                            #######       #######  \n");
    printk("###   ###                           ###     ###   ###     ###\n");
    printk("###  ###       ###                  ###     ###    ###       \n");
    printk("### ###        ###     ##########   ###     ###      ###     \n");
    printk("#######        ###     ###    ###   ###     ###         ###  \n");
    printk("###   ###      ###     ###    ###   ###     ###  ###     ### \n");
    printk("###    ###     ###     ###    ###     #######      #######   \n");
    printk("\n");
    printk("Ver.M\n");
    printk("@ 2021 kinpoko\n");
    printk("\n");
    printk("welcome to MikanOS!\n");

      


    AppMessage msg[1]; 
    int i = 1;
    while (true) {
    
    auto [ n, err ] = SyscallReceiveMessage(msg, 1);
    if (err) {
      printk("Receive message failed: %s\n", strerror(err));
      break;
    }
    if (msg[0].type == AppMessage::aMouseMove) {
        auto& arg = msg[0].arg.mouse_move;
        MouseObserver(arg.dx, arg.dy);
      
        
    }
    printk("roop is %d\n", i);
    ++i;
    
  }

    exit(0);

    
}