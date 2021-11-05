#include "system.hpp"

#include <cstring>
#include <limits>

#include "asmfunc.h"
#include "memory_manager.hpp"
#include "paging.hpp"
#include "shell.hpp"

/*--------------------------------------------------------------------------
 * functions to execute file
 *--------------------------------------------------------------------------
 */
namespace {

WithError<int> MakeArgVector(char *command, char *first_arg, char **argv,
                             int argv_len, char *argbuf, int argbuf_len) {
    int argc = 0;
    int argbuf_index = 0;

    auto push_to_argv = [&](const char *s) {
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
        return {argc, err};
    }
    if (!first_arg) {
        return {argc, MAKE_ERROR(Error::kSuccess)};
    }

    char *p = first_arg;
    while (true) {
        while (isspace(p[0])) {
            ++p;
        }
        if (p[0] == 0) {
            break;
        }
        const char *arg = p;

        while (p[0] != 0 && !isspace(p[0])) {
            ++p;
        }
        // here: p[0] == 0 || isspace(p[0])
        const bool is_end = p[0] == 0;
        p[0] = 0;
        if (auto err = push_to_argv(arg)) {
            return {argc, err};
        }
        if (is_end) {
            break;
        }
        ++p;
    }

    return {argc, MAKE_ERROR(Error::kSuccess)};
}

Elf64_Phdr *GetProgramHeader(Elf64_Ehdr *ehdr) {
    return reinterpret_cast<Elf64_Phdr *>(reinterpret_cast<uintptr_t>(ehdr) +
                                          ehdr->e_phoff);
}

uintptr_t GetFirstLoadAddress(Elf64_Ehdr *ehdr) {
    auto phdr = GetProgramHeader(ehdr);
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type != PT_LOAD) continue;
        return phdr[i].p_vaddr;
    }
    return 0;
}

static_assert(kBytesPerFrame >= 4096);

WithError<uint64_t> CopyLoadSegments(Elf64_Ehdr *ehdr) {
    auto phdr = GetProgramHeader(ehdr);
    uint64_t last_addr = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type != PT_LOAD) continue;

        LinearAddress4Level dest_addr;
        dest_addr.value = phdr[i].p_vaddr;
        last_addr = std::max(last_addr, phdr[i].p_vaddr + phdr[i].p_memsz);
        const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

        if (auto err = SetupPageMaps(dest_addr, num_4kpages, false)) {
            return {last_addr, err};
        }

        const auto src = reinterpret_cast<uint8_t *>(ehdr) + phdr[i].p_offset;
        const auto dst = reinterpret_cast<uint8_t *>(phdr[i].p_vaddr);
        memcpy(dst, src, phdr[i].p_filesz);
        memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
    }
    return {last_addr, MAKE_ERROR(Error::kSuccess)};
}

WithError<uint64_t> LoadELF(Elf64_Ehdr *ehdr) {
    if (ehdr->e_type != ET_EXEC) {
        return {0, MAKE_ERROR(Error::kInvalidFormat)};
    }

    const auto addr_first = GetFirstLoadAddress(ehdr);
    if (addr_first < 0xffff'8000'0000'0000) {
        return {0, MAKE_ERROR(Error::kInvalidFormat)};
    }

    return CopyLoadSegments(ehdr);
}

WithError<PageMapEntry *> SetupPML4(Task &current_task) {
    auto pml4 = NewPageMap();
    if (pml4.error) {
        return pml4;
    }

    const auto current_pml4 = reinterpret_cast<PageMapEntry *>(GetCR3());
    memcpy(pml4.value, current_pml4, 256 * sizeof(uint64_t));

    const auto cr3 = reinterpret_cast<uint64_t>(pml4.value);
    SetCR3(cr3);
    current_task.Context().cr3 = cr3;
    return pml4;
}

Error FreePML4(Task &current_task) {
    const auto cr3 = current_task.Context().cr3;
    current_task.Context().cr3 = 0;
    ResetCR3();

    const FrameID frame{cr3 / kBytesPerFrame};
    return memory_manager->Free(frame, 1);
}

/**
 * @brief set up a paging structure for the task and copy the LOAD segment into
 * memory
 *
 * @param file_entry
 * @param task task for server
 * @return WithError<AppLoadInfo>
 */
WithError<AppLoadInfo> LoadServer(fat::DirectoryEntry &file_entry, Task &task) {
    PageMapEntry *temp_pml4;

    // set up a new PML4. the first half is for OS the second half is for
    // application
    if (auto [pml4, err] = SetupPML4(task); err) {
        return {{}, err};
    } else {
        temp_pml4 = pml4;
    }

    std::vector<uint8_t> file_buf(file_entry.file_size);
    fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

    auto elf_header = reinterpret_cast<Elf64_Ehdr *>(&file_buf[0]);
    if (memcmp(elf_header->e_ident,
               "\x7f"
               "ELF",
               4) != 0) {
        return {{}, MAKE_ERROR(Error::kInvalidFile)};
    }

    auto [last_addr, err_load] = LoadELF(elf_header);
    if (err_load) {
        return {{}, err_load};
    }

    AppLoadInfo app_load{last_addr, elf_header->e_entry, temp_pml4};

    if (auto [pml4, err] = SetupPML4(task); err) {
        return {app_load, err};
    } else {
        app_load.pml4 = pml4;
    }
    auto err = CopyPageMaps(app_load.pml4, temp_pml4, 4, 256);
    return {app_load, err};
}

/**
 * @brief
 *
 * @param elf_header
 * @param task task for application
 * @return WithError<AppLoadInfo>
 */
WithError<AppLoadInfo> LoadApp(Elf64_Ehdr *elf_header, Task &task) {
    PageMapEntry *temp_pml4;

    if (auto [pml4, err] = SetupPML4(task); err) {
        return {{}, err};
    } else {
        temp_pml4 = pml4;
    }

    if (memcmp(elf_header->e_ident,
               "\x7f"
               "ELF",
               4) != 0) {
        return {{}, MAKE_ERROR(Error::kInvalidFile)};
    }

    auto [last_addr, err_load] = LoadELF(elf_header);
    if (err_load) {
        return {{}, err_load};
    }

    AppLoadInfo app_load{last_addr, elf_header->e_entry, temp_pml4};

    if (auto [pml4, err] = SetupPML4(task); err) {
        return {app_load, err};
    } else {
        app_load.pml4 = pml4;
    }
    auto err = CopyPageMaps(app_load.pml4, temp_pml4, 4, 256);
    return {app_load, err};
}
}  // namespace

WithError<int> ExecuteServer(fat::DirectoryEntry &file_entry, char *command,
                             char *first_arg) {
    __asm__("cli");
    auto &task = task_manager->CurrentTask();
    __asm__("sti");

    auto [app_load, err] = LoadServer(file_entry, task);
    if (err) {
        return {0, err};
    }

    task.SetName(command);

    LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
    if (auto err = SetupPageMaps(args_frame_addr, 1)) {
        return {0, err};
    }
    auto argv = reinterpret_cast<char **>(args_frame_addr.value);
    int argv_len = 32;  // argv = 8x32 = 256 bytes
    auto argbuf = reinterpret_cast<char *>(args_frame_addr.value +
                                           sizeof(char **) * argv_len);
    int argbuf_len = 4096 - sizeof(char **) * argv_len;
    auto argc =
        MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
    if (argc.error) {
        return {0, argc.error};
    }

    const int stack_size = 16 * 4096;
    LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'f000 - stack_size};
    if (auto err = SetupPageMaps(stack_frame_addr, stack_size / 4096)) {
        return {0, err};
    }

    const uint64_t elf_next_page =
        (app_load.vaddr_end + 4095) & 0xffff'ffff'ffff'f000;
    task.SetDPagingBegin(elf_next_page);
    task.SetDPagingEnd(elf_next_page);

    task.SetFileMapEnd(stack_frame_addr.value);

    int ret =
        CallApp(argc.value, argv, 3 << 3 | 3, app_load.entry,
                stack_frame_addr.value + 4096 - 8, &task.OSStackPointer());

    // task.Files().clear();
    task.FileMaps().clear();

    if (auto err = CleanPageMaps(LinearAddress4Level{0xffff'8000'0000'0000})) {
        return {ret, err};
    }

    return {ret, FreePML4(task)};
}

void TaskServer(uint64_t task_id, int64_t data) {
    const auto task_of_server_data = reinterpret_cast<DataOfServer *>(data);

    __asm__("cli");
    auto &task = task_manager->CurrentTask();
    __asm__("sti");

    auto [file_entry, post_slash] =
        fat::FindFile(task_of_server_data->file_name);
    if (!file_entry) {
        printk("[ kinOS ] no such server %s\n", task_of_server_data->file_name);
    } else {
        auto [ec, err] =
            ExecuteServer(*file_entry, task_of_server_data->file_name, "");
        if (err) {
            printk("[ kinOS ] cannnot execute server\n");
        }
    }
    while (true) __asm__("hlt");
}

WithError<int> ExecuteApp(Elf64_Ehdr *elf_header, char *first_arg) {
    __asm__("cli");
    auto &task = task_manager->CurrentTask();
    __asm__("sti");

    auto [app_load, err] = LoadApp(elf_header, task);
    if (err) {
        return {0, err};
    }

    LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
    if (auto err = SetupPageMaps(args_frame_addr, 1)) {
        return {0, err};
    }
    auto argv = reinterpret_cast<char **>(args_frame_addr.value);
    int argv_len = 32;  // argv = 8x32 = 256 bytes
    auto argbuf = reinterpret_cast<char *>(args_frame_addr.value +
                                           sizeof(char **) * argv_len);
    int argbuf_len = 4096 - sizeof(char **) * argv_len;
    auto argc = MakeArgVector("application", first_arg, argv, argv_len, argbuf,
                              argbuf_len);
    if (argc.error) {
        return {0, argc.error};
    }

    const int stack_size = 16 * 4096;
    LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'f000 - stack_size};
    if (auto err = SetupPageMaps(stack_frame_addr, stack_size / 4096)) {
        return {0, err};
    }

    const uint64_t elf_next_page =
        (app_load.vaddr_end + 4095) & 0xffff'ffff'ffff'f000;
    task.SetDPagingBegin(elf_next_page);
    task.SetDPagingEnd(elf_next_page);

    task.SetFileMapEnd(stack_frame_addr.value);

    int ret =
        CallApp(argc.value, argv, 3 << 3 | 3, app_load.entry,
                stack_frame_addr.value + 4096 - 8, &task.OSStackPointer());

    // task.Files().clear();
    task.FileMaps().clear();

    if (auto err = CleanPageMaps(LinearAddress4Level{0xffff'8000'0000'0000})) {
        return {ret, err};
    }

    return {ret, FreePML4(task)};
};

void TaskApp(uint64_t task_id, int64_t am_id) {
    __asm__("cli");
    auto &task = task_manager->CurrentTask();
    __asm__("sti");

    auto elf_header = reinterpret_cast<Elf64_Ehdr *>(&task.buf_[0]);

    auto [ec, err] = ExecuteApp(elf_header, task.arg_);
    if (err) {
        Message msg;
        msg.type = Message::kExcute;
        msg.arg.execute.success = false;
        msg.src_task = 1;
        msg.arg.execute.id = task.ID();
        task_manager->SendMessage(am_id, msg);

        while (true) {
            __asm__("cli");
            auto rmsg = task.ReceiveMessage();
            if (!rmsg) {
                task.Sleep();
                __asm__("sti");
                continue;
            }
            __asm__("sti");

            switch (rmsg->type) {
                case Message::kError:
                    if (rmsg->arg.error.retry) {
                        task_manager->SendMessage(am_id, msg);
                    }
                    break;
                case Message::kReceived:
                    goto end;

                default:
                    break;
            }
        }
    }

    Message msg;
    msg.type = Message::kExit;
    msg.src_task = 1;
    msg.arg.exit.id = task.ID();
    msg.arg.exit.result = ec;
    task_manager->SendMessage(am_id, msg);

    while (true) {
        __asm__("cli");
        auto rmsg = task.ReceiveMessage();
        if (!rmsg) {
            task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (rmsg->type) {
            case Message::kError:
                if (rmsg->arg.error.retry) {
                    task_manager->SendMessage(am_id, msg);
                }
                break;
            case Message::kReceived:
                printk("[ kinOS ] task %d is exited\n", task.ID());
                goto end;

            default:
                break;
        }
    }

end:
    while (true) __asm__("hlt");
}

uint8_t *v_image;

char kernel_log_buf[1024];
size_t kernel_log_head;
size_t kernel_log_tail;
bool kernel_log_changed;

void InitializeSystemTask(void *volume_image) {
    v_image = reinterpret_cast<uint8_t *>(volume_image);

    kernel_log_head = 0;
    kernel_log_tail = 0;
    kernel_log_changed = false;

    Task &os_task = task_manager->NewTask();

    auto os_server_data = new DataOfServer{
        "servers/gui",
    };
    os_task.InitContext(TaskServer, reinterpret_cast<uint64_t>(os_server_data))
        .Wakeup();

    Task &terminal_task = task_manager->NewTask();
    auto terminal_server_data = new DataOfServer{
        "servers/terminal",
    };
    terminal_task
        .InitContext(TaskServer,
                     reinterpret_cast<uint64_t>(terminal_server_data))
        .Wakeup();

    Task &am_task = task_manager->NewTask();
    auto am_server_data = new DataOfServer{
        "servers/am",
    };
    am_task.InitContext(TaskServer, reinterpret_cast<uint64_t>(am_server_data))
        .Wakeup();

    Task &fs_task = task_manager->NewTask();
    auto fs_server_data = new DataOfServer{
        "servers/fs",
    };
    fs_task.InitContext(TaskServer, reinterpret_cast<uint64_t>(fs_server_data))
        .Wakeup();

    Task &log_task = task_manager->NewTask();
    auto log_server_data = new DataOfServer{
        "servers/log",
    };
    log_task
        .InitContext(TaskServer, reinterpret_cast<uint64_t>(log_server_data))
        .Wakeup();
}

/**
 * @brief Copy the image volumed by the bootloader to buf
 *
 * @param buf
 * @param offset 512bytes uint
 * @param len 512bytes uint
 */
void ReadImage(void *buf, size_t offset, size_t len) {
    uint8_t *src_buf = reinterpret_cast<uint8_t *>(buf);
    size_t sector = offset * SECTOR_SIZE;
    size_t num_sector = len * SECTOR_SIZE;
    uint8_t *v_image_start = v_image + sector;
    memcpy(src_buf, v_image_start, num_sector);
}

void KernelLogWrite(char *s) {
    int i = kernel_log_head;
    if (kernel_log_changed) {
        kernel_log_head = kernel_log_tail;
    }

    while (*s) {
        kernel_log_buf[kernel_log_head] = *s;
        kernel_log_head = (kernel_log_head + 1) % sizeof(kernel_log_buf);
        ++s;
    }
    kernel_log_tail = kernel_log_head;
    kernel_log_head = i;
    kernel_log_changed = true;
}

size_t KernelLogRead(char *buf, size_t len) {
    size_t remaining = len;
    if (kernel_log_head > kernel_log_tail) {
        int copy_len =
            std::min(remaining, sizeof(kernel_log_buf) - kernel_log_head);
        memcpy(buf, &kernel_log_buf[kernel_log_head], copy_len);
        buf += copy_len;
        remaining -= copy_len;
        kernel_log_head = 0;
    }

    int copy_len = std::min(remaining, kernel_log_tail - kernel_log_head);
    memcpy(buf, &kernel_log_buf[kernel_log_head], copy_len);
    remaining -= copy_len;
    kernel_log_head = (kernel_log_head + copy_len) % sizeof(kernel_log_buf);
    kernel_log_changed = false;
    return remaining;
}

int printk(const char *format, ...) {
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    KernelLogWrite(s);
    return result;
}
