/**
 * @file system.hpp
 *
 * functions of system task(kernel task)
 */

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

void TaskServer(uint64_t task_id, int64_t data);

WithError<int> ExecuteServer(fat::DirectoryEntry &file_entry, char *command,
                             char *first_arg);

void TaskApp(uint64_t task_id, int64_t am_id);

WithError<int> ExecuteApp(Elf64_Ehdr *elf_header, char *first_arg);

extern uint8_t *v_image;

void InitializeSystemTask(void *volume_image);

void ReadImage(void *buf, size_t offset, size_t len);

extern char klog_buf[1024];  // kernel log
extern size_t klog_head;
extern size_t klog_tail;
extern bool klog_changed;

void klog_write(char *s);

size_t klog_read(char *buf, size_t len);

int printk(const char *format, ...);
