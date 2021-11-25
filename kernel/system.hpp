#pragma once

#include <deque>
#include <map>
#include <memory>
#include <optional>

#include "elf.hpp"
#include "fat.hpp"
#include "task.hpp"

#define SECTOR_SIZE 512

struct DataOfServer {
    char *file_name;
};

void TaskInitServer(uint64_t task_id, int64_t data);
void TaskServer(uint64_t task_id, int64_t init_id);
void TaskApp(uint64_t task_id, int64_t am_id);

WithError<int> ExecuteInitServer(fat::DirectoryEntry &file_entry, char *command,
                                 char *first_arg);
WithError<int> ExecuteServer(Elf64_Ehdr *elf_header, char *server_name);
WithError<int> ExecuteApp(Elf64_Ehdr *elf_header, char *command,
                          char *first_arg);

extern uint8_t *v_image;

void InitializeSystemTask(void *volume_image);

void ReadImage(void *buf, size_t offset_by_sector, size_t len_by_sector);
void CopyToImage(void *buf, size_t offset_by_sector, size_t len_by_sector);

extern char kernel_log_buf[1024];  // kernel log
extern size_t kernel_log_head;
extern size_t kernel_log_tail;
extern bool kernel_log_changed;

void KernelLogWrite(char *s);

size_t KernelLogRead(char *buf, size_t len);

int printk(const char *format, ...);
