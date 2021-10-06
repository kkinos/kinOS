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

#include "fat.hpp"
#include "shell.hpp"
#include "task.hpp"

#define SECTOR_SIZE 512

/**
 * @brief サーバーにわたす情報
 *
 */
struct DataOfServer {
    char *file_name;  // 実行するサーバーのファイル
};

/**
 * @brief サーバーを実行するタスク
 *
 * @param task_id
 * @param data DataOfServerのポインタ
 */
void TaskServer(uint64_t task_id, int64_t data);

/**
 * @brief サーバーを実行する
 *
 * @param file_entry
 * @param command
 * @param first_arg
 * @return WithError<int>
 */
WithError<int> ExecuteFile(fat::DirectoryEntry &file_entry, char *command,
                           char *first_arg);

extern uint8_t *v_image;

/**
 * @brief いくつかのサーバをあらかじめ起動しておく
 *
 *
 */
void InitializeSystemTask(void *volume_image);

/**
 * @brief ブートローダによってボリュームされたイメージをbufにコピーする
 *
 * @param buf
 * @param offset
 * @param len
 */
void ReadImage(void *buf, size_t offset, size_t len);

extern char klog_buf[1024];  // kernel log
extern size_t klog_head;     // kernel logの読み込み始め
extern size_t klog_tail;     // kernel logの読み込みおわり
extern bool klog_changed;    // kernel logが変化したかどうか

/**
 * @brief kernel logに書き込む
 *
 * @param s
 */
void klog_write(char *s);

/**
 * @brief kernel logを読む 返り値が0ならまだ読み込めていない部分がある
 *
 * @param buf
 * @param len
 * @return size_t
 */
size_t klog_read(char *buf, size_t len);

int printk(const char *format, ...);
