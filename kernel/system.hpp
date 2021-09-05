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
struct DataOfServer {
    char* file_name; // 実行するサーバーのファイル
};

/**
 * @brief サーバーを実行するタスク
 * 
 * @param task_id 
 * @param data DataOfServerのポインタ
 */
void TaskServer (
    uint64_t task_id, 
    int64_t data 
);

/**
 * @brief サーバーを実行する
 * 
 * @param file_entry 
 * @param command 
 * @param first_arg 
 * @return WithError<int> 
 */
WithError<int> ExecuteFile (
    fat::DirectoryEntry& file_entry, 
    char* command, 
    char* first_arg
);

extern uint8_t* v_image;

/**
 * @brief いくつかのサーバをあらかじめ起動しておく
 * 
 * 
 */
void InitializeSystemTask (
    void* volume_image
);

/**
 * @brief ブートローダによってボリュームされたイメージをコピーする
 * 
 * @param buf 
 * @param offset 
 * @param len 
 */
void ReadImage ( 
    void* buf,
    size_t offset, 
    size_t len
);
