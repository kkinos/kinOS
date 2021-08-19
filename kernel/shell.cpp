#include "shell.hpp"

#include <cstring>
#include <limits>

#include "font.hpp"
#include "layer.hpp"
#include "pci.hpp"
#include "asmfunc.h"
#include "elf.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"
#include "timer.hpp"
#include "keyboard.hpp"
#include "logger.hpp"
#include "console.hpp"

namespace {

    WithError<int> MakeArgVector(char* command, char* first_arg,
        char** argv, int argv_len, char* argbuf, int argbuf_len) {
    int argc = 0;
    int argbuf_index = 0;

    auto push_to_argv = [&](const char* s) {
        if (argc >= argv_len || argbuf_index >= argbuf_len) {
        return MAKE_ERROR(Error::kFull);
        }

        argv[argc] = &argbuf[argbuf_index];
        ++argc;
        strcpy(&argbuf[argbuf_index], s);
        argbuf_index += strlen(s) + 1;
        return MAKE_ERROR(Error::kSuccess);
    };

    if (auto err = push_to_argv(command)) {
        return { argc, err };
    }
    if (!first_arg) {
        return { argc, MAKE_ERROR(Error::kSuccess) };
    }

    char* p = first_arg;
    while (true) {
        while (isspace(p[0])) {
        ++p;
        }
        if (p[0] == 0) {
        break;
        }
        const char* arg = p;

        while (p[0] != 0 && !isspace(p[0])) {
        ++p;
        }
        // here: p[0] == 0 || isspace(p[0])
        const bool is_end = p[0] == 0;
        p[0] = 0;
        if (auto err = push_to_argv(arg)) {
        return { argc, err };
        }
        if (is_end) {
        break;
        }
        ++p;
    }

    return { argc, MAKE_ERROR(Error::kSuccess) };
}
    
    Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
        return reinterpret_cast<Elf64_Phdr*>(
        reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
    }

    uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
        auto phdr = GetProgramHeader(ehdr);
        for (int i = 0; i < ehdr->e_phnum; ++i) {
            if (phdr[i].p_type != PT_LOAD) continue;
            return phdr[i].p_vaddr;
        }
        return 0;
    }

    static_assert(kBytesPerFrame >= 4096);

    /**
     * @brief LOADセグメントを最終目的地にコピー
     * @param ehder ELFファイルのヘッダを指すポインタ
     * 
     */
    WithError<uint64_t> CopyLoadSegments(Elf64_Ehdr* ehdr) {
        auto phdr = GetProgramHeader(ehdr);
        uint64_t last_addr = 0;
        for (int i = 0; i < ehdr->e_phnum; ++i) {
            if (phdr[i].p_type != PT_LOAD) continue;

            LinearAddress4Level dest_addr;
            dest_addr.value = phdr[i].p_vaddr;
            last_addr = std::max(last_addr, phdr[i].p_vaddr + phdr[i].p_memsz);
            const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

            /*後半のページマップを設定しておく*/
            if (auto err = SetupPageMaps(dest_addr, num_4kpages, false)) {
            return { last_addr, err };
            }

            /*設定したページマップをもとにコピー*/
            const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr[i].p_offset;
            const auto dst = reinterpret_cast<uint8_t*>(phdr[i].p_vaddr);
            memcpy(dst, src, phdr[i].p_filesz);
            memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
        return { last_addr, MAKE_ERROR(Error::kSuccess) };
    }

    WithError<uint64_t> LoadELF(Elf64_Ehdr* ehdr) {
        if (ehdr->e_type != ET_EXEC) {
            return { 0, MAKE_ERROR(Error::kInvalidFormat) };
        }

        const auto addr_first = GetFirstLoadAddress(ehdr);
        if (addr_first < 0xffff'8000'0000'0000) {
             return { 0, MAKE_ERROR(Error::kInvalidFormat) };
        }

        return CopyLoadSegments(ehdr);


    }

    WithError<PageMapEntry*> SetupPML4(Task& current_task) {
        auto pml4 = NewPageMap();
        if (pml4.error) {
            return pml4;
        }

        const auto current_pml4 = reinterpret_cast<PageMapEntry*>(GetCR3());
        memcpy(pml4.value, current_pml4, 256 * sizeof(uint64_t));

        const auto cr3 = reinterpret_cast<uint64_t>(pml4.value);
        SetCR3(cr3);
        current_task.Context().cr3 = cr3;
        return pml4;
    }

    Error FreePML4(Task& current_task) {
        const auto cr3 = current_task.Context().cr3;
        current_task.Context().cr3 = 0;
        ResetCR3();

        const FrameID frame{cr3 / kBytesPerFrame};
        return memory_manager->Free(frame, 1);
    }

    void ListAllEntries(FileDescriptor& fd, uint32_t dir_cluster) {
        const auto kEntriesPerCluster =
            fat::bytes_per_cluster / sizeof(fat::DirectoryEntry);

        while (dir_cluster != fat::kEndOfClusterchain) {
            auto dir = fat::GetSectorByCluster<fat::DirectoryEntry>(dir_cluster);

            for (int i = 0; i < kEntriesPerCluster; ++i) {
            if (dir[i].name[0] == 0x00) {
                return;
            } else if (static_cast<uint8_t>(dir[i].name[0]) == 0xe5) {
                continue;
            } else if (dir[i].attr == fat::Attribute::kLongName) {
                continue;
            }

            char name[13];
            fat::FormatName(dir[i], name);
            PrintToFD(fd, "%s\n", name);
            }

            dir_cluster = fat::NextCluster(dir_cluster);
        }
        }
    
    /**
     * @brief タスクのページング構造を設定し、LOADセグメントをメモリにコピーする
     * 
     * @param file_entry 
     * @param task アプリを実行しようとしているタスク 
     * @return WithError<AppLoadInfo> 
     */
    WithError<AppLoadInfo> LoadApp(fat::DirectoryEntry& file_entry, Task& task) {
        PageMapEntry* temp_pml4;
        
        /*今のタスクのPML4を新たに設定する 前半はOS用 後半をアプリ用*/
        if (auto [ pml4, err ] = SetupPML4(task); err) {
            return { {}, err};
        } else {
            temp_pml4 = pml4;
        }

        /*もし予めLOADセグメントがあるのであればテーブル構造をコピーするだけ*/
        if (auto it = app_loads->find(&file_entry); it != app_loads->end()) {
            AppLoadInfo app_load = it->second;
            auto err = CopyPageMaps(temp_pml4, app_load.pml4, 4, 256);
            return { app_load, err };
        }

        std::vector<uint8_t> file_buf(file_entry.file_size);
        fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

        auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
        if (memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
            return { {}, MAKE_ERROR(Error::kInvalidFile) };
        }

        /*なければページ構造の後半を設定しLOADセグメントをそこへ配置する*/
        auto [ last_addr, err_load ] = LoadELF(elf_header);
        if (err_load) {
            return { {}, err_load };
        }

    
        AppLoadInfo app_load{last_addr, elf_header->e_entry, temp_pml4};
        app_loads->insert(std::make_pair(&file_entry, app_load));


        if (auto [ pml4, err ] = SetupPML4(task); err) {
            return { app_load, err };
        } else {
            app_load.pml4 = pml4;
        }
        auto err = CopyPageMaps(app_load.pml4, temp_pml4, 4, 256);
        return { app_load, err };
    }
}

std::map<fat::DirectoryEntry*, AppLoadInfo>* app_loads;

Shell::Shell(Task& task, const ShellDescriptor* sh_desc) 
    : task_{task} {
    if (!sh_desc->first_task) {
        for (int i = 0; i < files_.size(); ++i) {
            files_[i] = sh_desc->files[i];
        }
    } else {
        for (int i = 0; i < files_.size(); ++i) {
            files_[i] = std::make_shared<ShellFileDescriptor>(*this);
    }
    }

}


void Shell::InputKey(
    uint8_t modifier, uint8_t keycode, char ascii) {
        if (ascii == '\n') {
            ExecuteLine();
    } else {
        linebuf_[linebuf_index_] = ascii;
        ++linebuf_index_;
    }
}

void Shell::ExecuteLine() {
    char* command = &linebuf_[0];
    char* first_arg = strchr(&linebuf_[0], ' ');
    char* redir_char = strchr(&linebuf_[0], '>');
    char* pipe_char = strchr(&linebuf_[0], '|');
    if (first_arg) {
        *first_arg = 0;
        ++first_arg;
    }

    auto original_stdout = files_[1];
    int exit_code = 0;

    if (redir_char) {
        *redir_char = 0;
        char* redir_dest = &redir_char[1];
        while (isspace(*redir_dest)) {
            ++redir_dest;
        }

    auto [file, post_slash] = fat::FindFile(redir_dest);
    if (file == nullptr) {
        auto [new_file, err] = fat::CreateFile(redir_dest);
        if (err) {
            PrintToFD(*files_[2], 
                      "failed to create a redirect file: %s\n", err.Name());
            return;
        }
        file = new_file;
    } else if (file->attr == fat::Attribute::kDirectory || post_slash) {
        PrintToFD(*files_[2], "cannnot redirect to directory\n");
        return;
    }
    files_[1] = std::make_shared<fat::FileDescriptor>(*file);
    }

    std::shared_ptr<PipeDescriptor> pipe_fd;
    uint64_t subtask_id = 0;

    if (pipe_char) {
        *pipe_char = 0;
        char* subcommand = &pipe_char[1];
        while (isspace(*subcommand)) {
            ++subcommand;
        }

        auto& subtask = task_manager->NewTask();
        pipe_fd = std::make_shared<PipeDescriptor>(subtask);
        auto sh_desc = new ShellDescriptor{
            subcommand, true, false,
            { pipe_fd, files_[1], files_[2] }
        };
        files_[1] = pipe_fd;

        subtask_id = subtask
            .InitContext(TaskShell, reinterpret_cast<int64_t>(sh_desc))
            .Wakeup()
            .ID();
    }
 
    if (strcmp(command, "echo") == 0) {
        if (first_arg && first_arg[0] == '$') {
            if (strcmp(&first_arg[1], "?") == 0) {
            PrintToFD(*files_[1], "%d", last_exit_code_);
            }
        } else if (first_arg) {
            PrintToFD(*files_[1], "%s", first_arg);
        }

        PrintToFD(*files_[1], "\n");
    } else if (strcmp(command, "lspci") == 0) {
        char s[64];
        for (int i = 0; i < pci::num_device; ++i) {
        const auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        PrintToFD(*files_[1],
          "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
          dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
          dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
        } 
    } else if (strcmp(command, "ls") == 0) {
        if (!first_arg || first_arg[0] == '\0')  {
            ListAllEntries(*files_[1], fat::boot_volume_image->root_cluster);
        } else {
            auto [dir, post_slash] = fat::FindFile(first_arg);
            if (dir == nullptr) {
                PrintToFD(*files_[2], "No such file or directory: %s\n", first_arg);
                exit_code = 1;
            } else if (dir->attr == fat::Attribute::kDirectory) {
                ListAllEntries(*files_[1], dir->FirstCluster());
            } else {
                char name[13];
                fat::FormatName(*dir, name);
                if (post_slash) {
                    PrintToFD(*files_[2], "%s is not a directory\n", name);
                } else {
                    PrintToFD(*files_[1], "%s\n", name);
                }
            }
        }
    } else if (strcmp(command, "cat") == 0) {
        auto [ file_entry, post_slash] = fat::FindFile(first_arg);
        if (!file_entry) {
           PrintToFD(*files_[2], "no such file: %s\n", first_arg);
           exit_code = 1;
        } else if (file_entry->attr != fat::Attribute::kDirectory && post_slash) {
            char name[13];
            fat::FormatName(*file_entry, name);
            PrintToFD(*files_[2], "%s is not a directory\n", name);
            exit_code = 1;
        } else {
            auto cluster = file_entry->FirstCluster();
            auto remain_bytes = file_entry->file_size;

            while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
                char* p = fat::GetSectorByCluster<char>(cluster);

                int i = 0;
                for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i) {
                    PrintToFD(*files_[1], "%c", *p);
                    ++p;
                }
                remain_bytes -= i;
                cluster = fat::NextCluster(cluster);
            }
        }
    } else if (strcmp(command, "memstat") == 0) {
        const auto p_stat = memory_manager->Stat();
        PrintToFD(*files_[1], "Phys used : %lu frames (%llu MiB)\n",
            p_stat.allocated_frames,
            p_stat.allocated_frames * kBytesPerFrame / 1024 / 1024);
        PrintToFD(*files_[1], "Phys total: %lu frames (%llu MiB)\n",
            p_stat.total_frames,
            p_stat.total_frames * kBytesPerFrame / 1024 / 1024);
    } else if (command[0] != 0) {
        auto [ file_entry, post_slash ] = fat::FindFile(command);
        if (!file_entry) {
            PrintToFD(*files_[2], "no such command: %s\n", command);
            exit_code = 1;
        } else if (file_entry->attr != fat::Attribute::kDirectory && post_slash) {
            char name[13];
            fat::FormatName(*file_entry, name);
            PrintToFD(*files_[2], "%s is not a directory\n", name);
            exit_code = 1;
        } else {
            auto [ ec, err ] = ExecuteFile(*file_entry, command, first_arg);
            if (err) {
                PrintToFD(*files_[2], "failed to exec file: %s\n", err.Name());
                exit_code = -ec;
            } else {
            exit_code = ec;
            }
        }
    }

    if (pipe_fd) {
        pipe_fd->FinishWrite();
        __asm__("cli");
        auto [ ec , err ] = task_manager->WaitFinish(subtask_id);
        __asm__("sti");
        if (err) {
            Log(kWarn, "failed to wait finish: %s\n", err.Name());
        }
        exit_code = ec;
    }
    last_exit_code_ = exit_code;
    files_[1] = original_stdout;
}

WithError<int> Shell::ExecuteFile(fat::DirectoryEntry& file_entry, char* command, char* first_arg) {

    __asm__("cli");
    auto& task = task_manager->CurrentTask();
    __asm__("sti");

    auto [ app_load, err ] = LoadApp(file_entry, task);
    if (err) {
        return { 0, err };
    }
    
    task.SetCommandLine(command);
    

    /*アプリに渡す引数を予めメモリ上に確保しておきアプリからアクセスできるようにしておく*/
    LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
    if (auto err = SetupPageMaps(args_frame_addr, 1)) {
        return { 0, err };
    }
    auto argv = reinterpret_cast<char**>(args_frame_addr.value);
    int argv_len = 32; // argv = 8x32 = 256 bytes
    auto argbuf = reinterpret_cast<char*>(args_frame_addr.value + sizeof(char**) * argv_len);
    int argbuf_len = 4096 - sizeof(char**) * argv_len;
    auto argc = MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
    if (argc.error) {
        return { 0, argc.error };
    }

    const int stack_size = 8 * 4096;
    LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'e000};
    if (auto err = SetupPageMaps(stack_frame_addr, stack_size / 4096)) {
        return { 0, err };
    }

    for (int i = 0; i < files_.size(); ++i) {
        task.Files().push_back(files_[i]);
    }


    const uint64_t elf_next_page =
        (app_load.vaddr_end + 4095) & 0xffff'ffff'ffff'f000;
    task.SetDPagingBegin(elf_next_page);
    task.SetDPagingEnd(elf_next_page);

    task.SetFileMapEnd(0xffff'ffff'ffff'e000);



    int ret = CallApp(argc.value, argv, 3 << 3 | 3, app_load.entry,
            stack_frame_addr.value + 4096 - 8,
            &task.OSStackPointer());

    task.Files().clear();
    task.FileMaps().clear();
    
    if (auto err = CleanPageMaps(LinearAddress4Level{0xffff'8000'0000'0000})) {
        return { ret, err };
    }
    
    return { ret, FreePML4(task) };
}

void Shell::Print(char c) {
  char ch[] = {c, '\0'};
  printt(ch);
}

void Shell::Print(const char* s, std::optional<size_t> len) {
    if (len) {
        for (size_t i = 0; i < *len; ++i) {
            Print(*s);
            ++s;
        }
    } else {
        while (*s) {
            Print(*s);
            ++s;
        }
    }
}




void TaskShell(uint64_t task_id, int64_t data) {
    const auto sh_desc = reinterpret_cast<ShellDescriptor*>(data);

    __asm__("cli");
    Task& task = task_manager->CurrentTask();
    Shell* shell = new Shell{task, sh_desc};
    __asm__("sti");

    if (sh_desc && !sh_desc->command_line.empty()) {
        for (int i = 0; i < sh_desc->command_line.length(); ++i) {
            shell->InputKey(0, 0, sh_desc->command_line[i]);
        }

        shell->InputKey(0, 0, '\n');
    }

    if (sh_desc && sh_desc->exit_after_command) {
        delete sh_desc;
        __asm__("cli");
        task_manager->Finish(shell->LastExitCode());
        __asm__("sti");
    }

    auto add_blink_timer = [task_id](unsigned long t) {
        timer_manager->AddTimer(Timer{t + static_cast<int>(kTimerFreq * 0.5),
                                      1, task_id});
    };

    add_blink_timer(timer_manager->CurrentTick());

    bool window_isactive = false;

    while (true) {
        __asm__("cli");
        auto msg = task.ReceiveMessage();
        if (!msg) {
            task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (msg->type) {
            case Message::kTimerTimeout:
                add_blink_timer(msg->arg.timer.timeout);
                break;
            case Message::kKeyPush:
                if (msg->arg.keyboard.press) {
                    shell->InputKey(msg->arg.keyboard.modifier,
                                                         msg->arg.keyboard.keycode,
                                                         msg->arg.keyboard.ascii);
                }
                break;
            case Message::kWindowActive:
                window_isactive = msg->arg.window_active.activate;
                break;
            default:
                break;
        }
    }
}



ShellFileDescriptor::ShellFileDescriptor(Shell& shell)
    : shell_{shell} {
    }

size_t ShellFileDescriptor::Read(void* buf, size_t len) {
    char* bufc = reinterpret_cast<char*>(buf);

    while (true) {
        __asm__("cli");
        auto msg = shell_.UnderlyingTask().ReceiveMessage();
        __asm__("sti");
        if (!msg) {
            shell_.UnderlyingTask().Sleep();
            continue;
        }
        __asm__("sti");

        if (msg->type != Message::kKeyPush || !msg->arg.keyboard.press) {
            continue;
        }

        if (msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask)) {
            char s[3] = "^ ";
            s[1] = toupper(msg->arg.keyboard.ascii);
            printt(s);
            if (msg->arg.keyboard.keycode == 7 /* D */) {
                return 0; // EOT
            }
            continue;
        }

            bufc[0] = msg->arg.keyboard.ascii;
            printt(bufc);
           
            return 1;
    }
    
}

size_t ShellFileDescriptor::Write(const void* buf, size_t len) {
    shell_.Print(reinterpret_cast<const char*>(buf), len);
    return len;
}

size_t ShellFileDescriptor::Load(void* buf, size_t len, size_t offset) {
  return 0;
}

PipeDescriptor::PipeDescriptor(Task& task) : task_{task} {
}

size_t PipeDescriptor::Read(void* buf, size_t len) {
    if (len_ > 0) {
        const size_t copy_bytes = std::min(len_, len);
        memcpy(buf, data_, copy_bytes);
        len_ -= copy_bytes;
        memmove(data_, &data_[copy_bytes], len_);
        return copy_bytes;
    }

    if (closed_) {
        return 0;
    }

    while (true) {
        __asm__("cli");
        auto msg = task_.ReceiveMessage();
        if (!msg) {
            task_.Sleep();
            continue;
        }
        __asm__("sti");

        if (msg->type != Message::kPipe) {
            continue;
        }

        if (msg->arg.pipe.len == 0) {
            closed_ = true;
            return 0;
        }

        const size_t copy_bytes = std::min<size_t>(msg->arg.pipe.len, len);
        memcpy(buf, msg->arg.pipe.data, copy_bytes);
        len_ = msg->arg.pipe.len - copy_bytes;
        memcpy(data_, &msg->arg.pipe.data[copy_bytes], len_);
        return copy_bytes;

    }
}

size_t PipeDescriptor::Write(const void* buf, size_t len) {
    auto bufc = reinterpret_cast<const char*>(buf);
    Message msg{Message::kPipe};
    size_t sent_bytes = 0;
    while (sent_bytes < len) {
        msg.arg.pipe.len = std::min(len - sent_bytes, sizeof(msg.arg.pipe.data));
        memcpy(msg.arg.pipe.data, &bufc[sent_bytes], msg.arg.pipe.len);
        sent_bytes += msg.arg.pipe.len;
        __asm__("cli");
        task_.SendMessage(msg);
        __asm__("sti");
    }
    return len;
}

void PipeDescriptor::FinishWrite() {
  Message msg{Message::kPipe};
  msg.arg.pipe.len = 0;
  __asm__("cli");
  task_.SendMessage(msg);
  __asm__("sti");
}