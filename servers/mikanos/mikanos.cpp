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
#include "../../kernel/message.hpp"

unsigned int main_window_layer_id;
std::shared_ptr<ToplevelWindow> main_window;
void InitializeMainWindow() {

  main_window = std::make_shared<ToplevelWindow>(
      160, 68, "Hello Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();
  WriteString(*main_window->InnerWriter(), {0, 0}, "Welcome to", {0, 0, 0});
  WriteString(*main_window->InnerWriter(), {16, 16}, "MikanOS world!", {0, 0, 0});
  
  layer_manager->UpDown(main_window_layer_id, 2);
}

extern "C" int main() {
    InitializeGraphics();
    InitializeConsole();
    InitializeLayer();
    InitializeMainWindow();
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

    layer_manager->Draw({{0, 0}, ScreenSize()});


      


    Message msg[1]; 
    int i = 1;
    while (true) {
    
    auto [ n, err ] = SyscallReceiveMessage(msg, 1);
    if (err) {
      printk("Receive message failed: %s\n", strerror(err));
      break;
    }
    if (msg[0].type == Message::aMouseMove) {

      auto& arg = msg[0].arg.mouse_move;
      mouse->OnInterrupt(arg.buttons, arg.dx, arg.dy);
      
    } else if (msg[0].type == Message::aOpenWindow) {

      auto& arg = msg[0].arg.openwindow;

      const auto win = std::make_shared<ToplevelWindow>(
      arg.w, arg.h, "chinchin");

    const auto layer_id = layer_manager->NewLayer()
      .SetWindow(win)
      .SetDraggable(true)
      .Move({arg.x, arg.y})
      .ID();
    active_layer->Activate(layer_id);

    }

    printk("roop is %d\n", i);
    ++i;
    
  }

    exit(0);

    
}