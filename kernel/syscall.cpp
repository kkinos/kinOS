#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>
#include <cmath>
#include <fcntl.h>


#include "asmfunc.h"
#include "msr.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "shell.hpp"
#include "font.hpp"
#include "timer.hpp"
#include "keyboard.hpp"
#include "console.hpp"
#include "layer.hpp"

namespace syscall {
  struct Result {
    uint64_t value;
    int error;
  };

#define SYSCALL(name) \
  Result name( \
      uint64_t arg1, uint64_t arg2, uint64_t arg3, \
      uint64_t arg4, uint64_t arg5, uint64_t arg6)

SYSCALL(LogString) {
  if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
    return { 0, EPERM };
  }
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = strlen(s);
  if (len > 1024) {
    return { 0, E2BIG };
  }
  Log(static_cast<LogLevel>(arg1), "%s", s);
  return { len, 0 };
}

SYSCALL(PutString) {
  const auto fd = arg1;
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = arg3;
  if (len > 1024) {
    return { 0, E2BIG };
  }

  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  if (fd < 0 || task.Files().size() <= fd || !task.Files()[fd]) {
    return { 0, EBADF };
  }
  return { task.Files()[fd]->Write(s, len), 0 };
}

SYSCALL(Exit) {
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");
  return { task.OSStackPointer(), static_cast<int>(arg1) };
}



SYSCALL(GetCurrentTick) {
  return { timer_manager->CurrentTick(), kTimerFreq };
}

SYSCALL(CreateTimer) {
  const unsigned int mode = arg1;
  const int timer_value = arg2;
  if (timer_value <= 0) {
    return { 0, EINVAL };
  }

  __asm__("cli");
  const uint64_t task_id = task_manager->CurrentTask().ID();
  __asm__("sti");

  unsigned long timeout = arg3 * kTimerFreq / 1000;
  if (mode & 1) { // relative
    timeout += timer_manager->CurrentTick();
  }

  __asm__("cli");
  timer_manager->AddTimer(Timer{timeout, -timer_value, task_id});
  __asm__("sti");
  return { timeout * 1000 / kTimerFreq, 0 };
}

namespace {
  size_t AllocateFD(Task& task) {
    const size_t num_files = task.Files().size();
    for (size_t i = 0; i < num_files; ++i) {
      if (!task.Files()[i]) {
        return i;
      }
    }
    task.Files().emplace_back();
    return num_files;
  }

  std::pair<fat::DirectoryEntry*, int> CreateFile(const char* path) {
    auto [ file, err ] = fat::CreateFile(path);
    switch (err.Cause()) {
    case Error::kIsDirectory: return { file, EISDIR };
    case Error::kNoSuchEntry: return { file, ENOENT };
    case Error::kNoEnoughMemory: return { file, ENOSPC };
    default: return { file, 0 };
    }
    }

} // namespace

SYSCALL(OpenFile) {
  const char* path = reinterpret_cast<const char*>(arg1);
  const int flags = arg2;
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

   if (strcmp(path, "@stdin") == 0) {
    return { 0, 0 };
  }

  auto [ file, post_slash ] = fat::FindFile(path);
  if (file == nullptr) {
    if ((flags & O_CREAT) == 0) {
      return { 0, ENOENT };
    }
    auto [ new_file, err ] = CreateFile(path);
    if (err) {
      return { 0, err };
    }
    file = new_file;
  } else if (file->attr != fat::Attribute::kDirectory && post_slash) {
    return { 0, ENOENT };
  }

  size_t fd = AllocateFD(task);
  task.Files()[fd] = std::make_unique<fat::FileDescriptor>(*file);
  return { fd, 0 };
}

SYSCALL(ReadFile) {
  const int fd = arg1;
  void* buf = reinterpret_cast<void*>(arg2);
  size_t count = arg3;
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  if (fd < 0 || task.Files().size() <= fd || !task.Files()[fd]) {
    return { 0, EBADF };
  }
  return { task.Files()[fd]->Read(buf, count), 0 };
}

SYSCALL(DemandPages) {
  const size_t num_pages = arg1;
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  const uint64_t dp_end = task.DPagingEnd();
  task.SetDPagingEnd(dp_end + 4096 * num_pages);
  return { dp_end, 0};
}

SYSCALL(MapFile) {
  const int fd = arg1;
  size_t *file_size = reinterpret_cast<size_t*>(arg2);
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  if (fd < 0 || task.Files().size() <= fd || !task.Files()[fd]) {
    return { 0, EBADF};
  }

  *file_size = task.Files()[fd]->Size();
  const uint64_t vaddr_end = task.FileMapEnd();
  const uint64_t vaddr_begin = (vaddr_end - *file_size) & 0xffff'ffff'ffff'f000;
  task.SetFileMapEnd(vaddr_begin);
  task.FileMaps().push_back(FileMapping{fd, vaddr_begin, vaddr_end});
  return { vaddr_begin, 0 };

}

SYSCALL(CreateAppTask) {
  char* command_line = reinterpret_cast<char*>(arg1);

  __asm__("cli");
  auto& parent_task = task_manager->CurrentTask();
  __asm__("sti");

  auto& child_task = task_manager->NewTask();
  
  child_task.SetPID(parent_task.ID())
    .SetCommandLine(command_line);
    
  Message msg{Message::kCreateAppTask};
  msg.arg.create.pid = parent_task.ID();
  msg.arg.create.cid = child_task.ID();

  __asm__("cli");
  task_manager->SendMessage(1,msg);    
  __asm__("sti");
  
  return {child_task.ID(), 0};
}

SYSCALL(ReceiveMessage) {
  const auto receive_message = reinterpret_cast<Message*>(arg1);
  const size_t len = arg2;

  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");
  size_t i = 0;

  while (i < len) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg && i == 0) {
      task.Sleep();
      continue;
    }
    __asm__("sti");

    if (!msg) {
      break;
    }

    receive_message[i].type = msg->type;
    receive_message[i].src_task = msg->src_task;
    receive_message[i].arg = msg->arg;
    ++i;
      
    }
    return { i , 0};
  }

SYSCALL(SendMessageToOs) {
  const auto send_message = reinterpret_cast<Message*>(arg1);

  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  Message msg;
  msg = *send_message;
  msg.src_task = task.ID();

  __asm__("cli");
  task_manager->SendMessageToOs(msg);   
  __asm__("sti");
    
  return { 0, 0 };
}


SYSCALL(SendMessageToTask) {
  const auto send_message = reinterpret_cast<Message*>(arg1);
  int task_id = arg2;

  Message msg;
  msg = *send_message;
  __asm__("cli");
  task_manager->SendMessage(task_id, msg);   
  __asm__("sti");
    

return { 0, 0 };
}

SYSCALL(WritePixel) {
  const int x = arg1;
  const int y = arg2;
  const uint8_t r = arg3;
  const uint8_t g = arg4; 
  const uint8_t b = arg5;
  screen_writer->Write({ x , y }, PixelColor{ r, g, b });
  return {0, 0};
}

SYSCALL(FrameBufferWitdth) {
  uint64_t w = screen_writer->Width();
  return { w, 0 };
}

SYSCALL(FrameBufferHeight) {
  uint64_t h = screen_writer->Height();
  return { h, 0};
}

SYSCALL(CopyToFrameBuffer) {
  
  const auto src_buf = reinterpret_cast<uint8_t*>(arg1);
  int copy_start_x = arg2;
  int copy_start_y = arg3;
  int bytes_per_copy_line = arg4;
  uint8_t* dst_buf = screen->Config().frame_buffer + 4 *
    (screen->Config().pixels_per_scan_line * copy_start_y + copy_start_x);
  memcpy(dst_buf, src_buf, bytes_per_copy_line);
  
  return{ 0, 0 };

}


#undef SYSCALL

} 

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 0x11> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::GetCurrentTick,
  /* 0x04 */ syscall::CreateTimer,
  /* 0x05 */ syscall::OpenFile,
  /* 0x06 */ syscall::ReadFile,
  /* 0x07 */ syscall::DemandPages,
  /* 0x08 */ syscall::MapFile,
  /* 0x09 */ syscall::CreateAppTask,
  /* 0x0a */ syscall::ReceiveMessage,
  /* 0x0b */ syscall::SendMessageToOs,
  /* 0x0c */ syscall::SendMessageToTask,
  /* 0x0d */ syscall::WritePixel,
  /* 0x0e */ syscall::FrameBufferWitdth,
  /* 0x0f */ syscall::FrameBufferHeight,
  /* 0x10 */ syscall::CopyToFrameBuffer,
 

};

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u);
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                       static_cast<uint64_t>(16 | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}