/**
 * @file mikanos.cpp
 *
 * MikanOS版のOSサーバー
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "mouse.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "shadow_buffer.hpp"
#include "../../libs/common/message.hpp"

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
  
  window_layer_id.push_back(main_window_layer_id);
  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

void PrintMessage() { 
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
}

extern "C" int main() {
    InitializeGraphics();
    InitializeConsole();
    InitializeLayer();
    InitializeMainWindow();
    InitializeMouse();
    PrintMessage();
    layer_manager->Draw({{0, 0}, ScreenSize()});

    layer_task_map = new std::map<unsigned int, uint64_t>;

    Message msg[1]; 
    int i = 1;
    while (true) {
      
      auto [ n, err ] = SyscallReceiveMessage(msg, 1);
      if (err) {
        printk("Receive message failed: %s\n", strerror(err));
      }

      if (msg[0].type == Message::aMouseMove) {
        auto& arg = msg[0].arg.mouse_move;
        mouse->OnInterrupt(arg.buttons, arg.dx, arg.dy);
      } else if (msg[0].type == Message::aKeyPush) {
        if (msg[0].arg.keyboard.ascii == '\t') {
          if (msg[0].arg.keyboard.press) {
            auto act_id = active_layer->GetActive();
            unsigned int next_act_window_id = window_layer_id[0];
            
            for (int i = 0; i < window_layer_id.size(); ++i) {
              if (act_id == window_layer_id[i]) {
                if (i == window_layer_id.size() - 1) {
                  next_act_window_id = window_layer_id[0];
                } else {
                  next_act_window_id = window_layer_id[i + 1];
                  }
              }
            }
            active_layer->Activate(next_act_window_id);
          }
        } else if (msg->arg.keyboard.press && 
                   msg[0].arg.keyboard.keycode == 59 /* F2 */) {
          console->Clear();
        } else {
          auto act = active_layer->GetActive();
          auto task_it = layer_task_map->find(act);
          if (task_it != layer_task_map->end()) {
            if (msg[0].arg.keyboard.keycode == 20 /* Q key */ &&
                msg[0].arg.keyboard.modifier & (1 | 16)) {
                  Message rmsg{Message::aQuit};
                  SyscallSendMessage(&rmsg, task_it->second);
                } else {
                  SyscallSendMessage(&msg[0], task_it->second);
                }
          } else {
            printk("key push not handled: keycode %02x, ascii %02x\n",
            msg[0].arg.keyboard.keycode,
            msg[0].arg.keyboard.ascii);
        }
      }
        
    
      } else if (msg[0].type == Message::aOpenWindow) {
        auto& arg = msg[0].arg.openwindow;
        const auto task_id = msg[0].src_task;
        const auto win = std::make_shared<ToplevelWindow>(arg.w, arg.h, "");
        const auto layer_id = layer_manager->NewLayer()
          .SetWindow(win)
          .SetDraggable(true)
          .Move({arg.x, arg.y})
          .ID();
        
        active_layer->Activate(layer_id);
        window_layer_id.push_back(layer_id);
        
        Message rmsg{Message::aLayerId};
        rmsg.arg.layerid.layerid = layer_id;
        SyscallSendMessage(&rmsg, task_id);
        
        layer_task_map->insert(std::make_pair(layer_id, task_id));
        
      } else if (msg[0].type == Message::aWinFillRectangle) {
        auto& arg = msg[0].arg.winfillrectangle;
        auto layer = layer_manager->FindLayer(arg.layer_id);
         if (layer == nullptr) {
          continue;
        }
        FillRectangle(*layer->GetWindow()->Writer(), {arg.x, arg.y}, {arg.w, arg.h}, ToColor(arg.color));
        if (msg[0].arg.winfillrectangle.draw) {
          layer_manager->Draw(arg.layer_id);
        }  
      } else if (msg[0].type == Message::aWinWriteChar) {
        auto& arg = msg[0].arg.winwritechar;
        auto layer = layer_manager->FindLayer(arg.layer_id);
        if (layer == nullptr) {
          continue;
        }
        WriteString(*layer->GetWindow()->Writer(), {arg.x, arg.y}, &arg.c ,ToColor(arg.color));
        if (arg.draw) {
        layer_manager->Draw(arg.layer_id);
        }
      } else if (msg[0].type == Message::aWinDrawLine) {
         auto sign = [](int x) {
          return (x > 0) ? 1 : (x < 0) ? -1 : 0;
        };
        auto& arg = msg[0].arg.windrawline;
        auto layer = layer_manager->FindLayer(arg.layer_id);
        if (layer == nullptr) {
          continue;
        }
        auto win = layer->GetWindow();
        const int dx = arg.x1 - arg.x0 + sign(arg.x1 - arg.x0);
        const int dy = arg.y1 - arg.y0 + sign(arg.y1 - arg.y0);

        if (dx == 0 && dy == 0) {
          win->Writer()->Write({arg.x0, arg.y0}, ToColor(arg.color));
          continue;
        }

        const auto floord = static_cast<double(*)(double)>(floor);
        const auto ceild = static_cast<double(*)(double)>(ceil);

        if (abs(dx) >= abs(dy)) {
          if (dx < 0) {
            std::swap(arg.x0, arg.x1);
            std::swap(arg.y0, arg.y1);
          }
          const auto roundish = arg.y1 >= arg.y0 ? floord : ceild;
          const double m = static_cast<double>(dy) / dx;
          for (int x = arg.x0; x <= arg.x1; ++x) {
            const int y = roundish(m * (x - arg.x0) + arg.y0);
            win->Writer()->Write({x, y}, ToColor(arg.color));
          }
        } else {
          if (dy < 0) {
            std::swap(arg.x0, arg.x1);
            std::swap(arg.y0, arg.y1);
          }
          const auto roundish = arg.x1 >= arg.x0 ? floord : ceild;
          const double m = static_cast<double>(dx) / dy;
          for (int y = arg.y0; y <= arg.y1; ++y) {
            const int x = roundish(m * (y - arg.y0) + arg.x0);
            win->Writer()->Write({x, y}, ToColor(arg.color));
          }
        }

        if (arg.draw) {
          layer_manager->Draw(arg.layer_id);
        }

      } else if (msg[0].type == Message::aWinMoveRec) {
        auto& arg = msg[0].arg.winmoverec;
        auto layer = layer_manager->FindLayer(arg.layer_id);
         if (layer == nullptr) {
          continue;
        }
        auto win = layer->GetWindow();
        win->Move(Vector2D<int>{arg.x0, arg.y0}, Rectangle<int>{{arg.rx0, arg.ry0},{arg.rx1, arg.ry1}});

        if (arg.draw) {
          layer_manager->Draw(arg.layer_id);
        }
        
      } else if (msg[0].type == Message::aWinRedraw) {
        auto& arg = msg[0].arg.layerid;
        layer_manager->Draw(arg.layerid);
      } else if (msg[0].type == Message::aCloseWindow) {
        auto& arg = msg[0].arg.layerid;
        auto layer = layer_manager->FindLayer(arg.layerid);
         if (layer == nullptr) {
          continue;
        }
        const auto layer_pos = layer->GetPosition();
        const auto win_size = layer->GetWindow()->Size();
        active_layer->Activate(0);
        layer_manager->RemoveLayer(arg.layerid);
        layer_manager->Draw({layer_pos, win_size});
        layer_task_map->erase(arg.layerid);

      }

     

  }

    exit(0);

    
}