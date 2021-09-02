/**
 * @file system.hpp
 *
 * systemtaskの関数
 */

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include "task.hpp"
#include "fat.hpp"
#include "shell.hpp"


WithError<int> ExecuteFile(Task& task, fat::DirectoryEntry& file_entry, char* command, char* first_arg);

/*--------------------------------------------------------------------------
 * Server実行時にわたす情報
 *--------------------------------------------------------------------------
 */
struct DataOfServer {
    char* command_line;
};


/*--------------------------------------------------------------------------
 * Serverを実行するタスク
 *--------------------------------------------------------------------------
 */
void TaskOfServer(uint64_t task_id, int64_t data);