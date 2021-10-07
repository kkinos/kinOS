/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <limits>
#include <numeric>
#include <vector>

#include "../libs/common/message.hpp"
#include "acpi.hpp"
#include "asmfunc.h"
#include "fat.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "keyboard.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "mouse.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "segment.hpp"
#include "shell.hpp"
#include "syscall.hpp"
#include "system.hpp"
#include "task.hpp"
#include "timer.hpp"
#include "usb/xhci/xhci.hpp"

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

void TaskIdle(uint64_t task_id, int64_t data) {
    while (true) __asm__("hlt");
}

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref, const acpi::RSDP& acpi_table,
    void* volume_image) {
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

        Message rmsg;
        uint64_t rid;

        switch (msg->type) {
            case Message::kInterruptXHCI:
                usb::xhci::ProcessEvents();
                break;

            case Message::kExpandTaskBuffer:
                task_manager->ExpandTaskBuffer(msg->arg.expand.task_id,
                                               msg->arg.expand.bytes);
                rmsg.type = Message::kExpandTaskBuffer;
                rmsg.src_task = 1;
                __asm__("cli");
                task_manager->SendMessage(msg->src_task, rmsg);
                __asm__("sti");

                printk("[ kinOS ] Expand task %d buffer\n",
                       msg->arg.expand.task_id);
                break;

            default:
                printk("[ kinOS ] Unknown message type: %d\n", msg->type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}