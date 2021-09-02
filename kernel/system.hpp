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
void TaskOfServer (
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

/**
 * @brief いくつかのサーバをあらかじめ起動しておく
 * 
 * 
 */
void StartSomeServers();
