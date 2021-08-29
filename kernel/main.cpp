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

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  return result;
}

extern "C" void KernelMainNewStack(
  const FrameBufferConfig& frame_buffer_config_ref,
  const MemoryMap& memory_map_ref,
  const acpi::RSDP& acpi_table,
  void* volume_image) {
  MemoryMap memory_map{memory_map_ref};

  InitializeGraphics(frame_buffer_config_ref);
  SetLogLevel(kWarn);

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeTSS();
  InitializeInterrupt();

  fat::Initialize(volume_image);
  InitializePCI();

  screen = new FrameBuffer;
  screen->Initialize(screen_config);

  acpi::Initialize(acpi_table);
  InitializeLAPICTimer();

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

  char str[128];

  while (true) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

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