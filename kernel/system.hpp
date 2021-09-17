/**
 * @file system.hpp
 *
 * システムタスクの関数群
 */

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include "task.hpp"
#include "fat.hpp"
#include "shell.hpp"

#define SECTOR_SIZE 512

/**
 * @brief サーバーにわたす情報
 * 
 */
struct DataOfServer
{
    char *file_name; // 実行するサーバーのファイル
};

void TaskServer(uint64_t task_id, int64_t data);

WithError<int> ExecuteFile(fat::DirectoryEntry &file_entry, char *command, char *first_arg);

extern uint8_t *v_image;

void InitializeSystemTask(void *volume_image);

void ReadImage(void *buf, size_t offset, size_t len);

extern char klog_buf[1024]; // kernel log
extern size_t klog_head;    // kernel logの読み込み始め
extern size_t klog_tail;    // kernel logの読み込みおわり
extern bool klog_changed;    // kernel logが変化したかどうか

void klog_write(char *s);
size_t klog_read(char *buf, size_t len);

int printk(const char *format, ...);
