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
#include "pci.hpp"
#include "logger.hpp"
#include "usb/xhci/xhci.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "../libs/common/message.hpp"
#include "timer.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"
#include "task.hpp"
#include "shell.hpp"
#include "fat.hpp"
#include "syscall.hpp"
#include "system.hpp"

alignas(16) uint8_t kernel_main_stack[1024 * 1024];


void TaskIdle(uint64_t task_id, int64_t data) {
        while (true) __asm__("hlt");
    }

extern "C" void KernelMainNewStack(
  const FrameBufferConfig& frame_buffer_config_ref,
  const MemoryMap& memory_map_ref,
  const acpi::RSDP& acpi_table,
  void* volume_image
  )
  {
    MemoryMap memory_map{memory_map_ref};

    SetLogLevel(kWarn);

    InitializeSegmentation();
    InitializePaging();
    InitializeMemoryManager(memory_map);
    InitializeTSS();
    InitializeInterrupt();

    InitializePCI();

    acpi::Initialize(acpi_table);
    InitializeLAPICTimer();

    /* 画面領域の設定 */
    InitializeGraphics(frame_buffer_config_ref);
    screen = new FrameBuffer;
    screen->Initialize(screen_config);

    InitializeSyscall();

    InitializeTask();


    Task& system_task = task_manager->CurrentTask();
    system_task.SetCommandLine("systemtask");

    usb::xhci::Initialize();
    InitializeKeyboard();
    InitializeMouse();

    app_loads = new std::map<fat::DirectoryEntry*, AppLoadInfo>;

    fat::Initialize(volume_image);

    InitializeSystemTask(volume_image);


    while (true) {
      __asm__("cli");
      const auto tick = timer_manager->CurrentTick();
      __asm__("sti");

      __asm__("cli");
      auto msg = system_task.ReceiveMessage();
      if (!msg) {
        system_task.Sleep();
        __asm__("sti");
        continue;
    
      }
      __asm__("sti");
      
      switch (msg->type) {

      case Message::kInterruptXHCI:
        usb::xhci::ProcessEvents();
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