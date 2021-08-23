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

unsigned int main_window_layer_id;
void InitializeMainWindow() {
  auto main_window = std::make_shared<Window>(
      160, 68);
    DrawWindow(*main_window->Writer(), "Hello Window");
    WriteString(*main_window->Writer(), {24, 28}, "Welcome to", {0, 0, 0});
    WriteString(*main_window->Writer(), {24, 44}, "MikanOS world!", {0, 0, 0});

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .Move({300, 300})
    .ID();
  
  layer_manager->UpDown(main_window_layer_id, 1);
  layer_manager->Draw();
}

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

    InitializeMainWindow();


      


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
      mouse->OnInterrupt(arg.buttons, arg.dx, arg.dy);
      
    }
    printk("roop is %d\n", i);
    ++i;
    
  }

    exit(0);

    
}