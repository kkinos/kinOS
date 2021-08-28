/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>
#include <deque>
#include <limits>

#include "frame_buffer_config.hpp"
#include "memory_map.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/xhci/xhci.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "../libs/common/message.hpp"
#include "timer.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"
#include "task.hpp"
#include "shell.hpp"
#include "fat.hpp"
#include "syscall.hpp"

std::shared_ptr<ToplevelWindow> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
  main_window = std::make_shared<ToplevelWindow>(
      160, 52, screen_config.pixel_format, "Hello Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();

  window_layer_id.push_back(main_window_layer_id);

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}


alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
  const FrameBufferConfig& frame_buffer_config_ref,
  const MemoryMap& memory_map_ref,
  const acpi::RSDP& acpi_table,
  void* volume_image) {
  MemoryMap memory_map{memory_map_ref};

  InitializeGraphics(frame_buffer_config_ref);
  InitializeConsole();

  SetLogLevel(kWarn);

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeTSS();
  InitializeInterrupt();

  fat::Initialize(volume_image);
  InitializePCI();

  InitializeLayer();
  InitializeMainWindow();
  layer_manager->Draw({{0, 0}, ScreenSize()});
  
  acpi::Initialize(acpi_table);
  InitializeLAPICTimer();

  const int kTextboxCursorTimer = 1;
  const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
  timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTimer, 1});
  bool textbox_cursor_visible = false;

  InitializeSyscall();

  InitializeTask();
  Task& main_task = task_manager->CurrentTask();
  main_task.SetCommandLine("kinOS");

  Task& os_task = task_manager->NewTask();
  task_manager->SetOsTaskId(os_task.ID());

  usb::xhci::Initialize();
  InitializeKeyboard();
  InitializeMouse();

  app_loads = new std::map<fat::DirectoryEntry*, AppLoadInfo>;

  auto os_sh_desc = new ShellDescriptor{
    "servers/mikanos", true, true,
    { nullptr, nullptr, nullptr }
  };

  os_task.InitContext(TaskShell, reinterpret_cast<uint64_t>(os_sh_desc)).Wakeup();

  auto sh_desc = new ShellDescriptor{
    "servers/terminal", true, true,
    { nullptr, nullptr, nullptr }
  };

  Task& task = task_manager->NewTask()
                .InitContext(TaskShell, reinterpret_cast<uint64_t>(sh_desc))
                .Wakeup();


  
  printk("\n");
  printk("###    ###                            #######       #######  \n");
  printk("###   ###                           ###     ###   ###     ###\n");
  printk("###  ###       ###                  ###     ###    ###       \n");
  printk("### ###        ###     ##########   ###     ###      ###     \n");
  printk("#######        ###     ###    ###   ###     ###         ###  \n");
  printk("###   ###      ###     ###    ###   ###     ###  ###     ### \n");
  printk("###    ###     ###     ###    ###     #######      #######   \n");
  printk("\n");
  printk("Ver.MIKANOS\n");
  printk("@ 2021 kinpoko\n");
  printk("\n");
  printt("welcome to kinOS!!\n");

  char str[128];

   while (true) {

    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    sprintf(str, "%010lu", tick);
    FillRectangle(*main_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*main_window->InnerWriter(), {20, 4}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    __asm__("cli");
    auto msg = main_task.ReceiveMessage();
    if (!msg) {
      main_task.Sleep();
      __asm__("sti");
      continue;
  
    }

    __asm__("sti");
    

    switch (msg->type) {
    case Message::kInterruptXHCI:
      usb::xhci::ProcessEvents();
      break;
    case Message::kTimerTimeout:
      if (msg->arg.timer.value == kTextboxCursorTimer) {
        __asm__("cli");
        timer_manager->AddTimer(
            Timer{msg->arg.timer.timeout + kTimer05Sec, kTextboxCursorTimer, 1});
        __asm__("sti");
      }
      break;
    
    case Message::kKeyPush:
      /*タブキーを押したときにアクティブなウィンドウを切り替える*/
      if (msg->arg.keyboard.ascii == '\t') {
        if (msg->arg.keyboard.press) {
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
                 msg->arg.keyboard.keycode == 59 /* F2 */) {
        console->Clear();
      } else {
          auto act = active_layer->GetActive();
          __asm__("cli");
          auto task_it = layer_task_map->find(act);
          __asm__("sti");
          if (task_it != layer_task_map->end()) {
            __asm__("cli");
            task_manager->SendMessage(task_it->second, *msg);
            __asm__("sti");
          } else {
            printt("key push not handled: keycode %02x, ascii %02x\n",
              msg->arg.keyboard.keycode,
              msg->arg.keyboard.ascii);
          }
        }
      break;
  
    case Message::kLayer:
      ProcessLayerMessage(*msg);
      __asm__("cli");
      task_manager->SendMessage(msg->src_task, Message{Message::kLayerFinish});
      __asm__("sti");
      break;

    case Message::kCreateAppTask:
      task_manager->CreateAppTask(msg->arg.create.pid, msg->arg.create.cid);
      break;
      
    default:
      Log(kError, "Unknown message type: %d\n", msg->type);
    }
  }
  }
  
  
extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}