/**
 * @file am.hpp
 *
 * @brief application management server
 *
 */

#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "../../libs/common/template.hpp"
#include "../../libs/gui/guisyscall.hpp"

class AppInfo {
   public:
    AppInfo(uint64_t task_id, uint64_t p_task_id);
    uint64_t ID() { return task_id_; }
    uint64_t PID() { return p_task_id_; }

   private:
    uint64_t task_id_;
    uint64_t p_task_id_;
};

class AppManager {
   public:
    AppManager();
    void NewApp(uint64_t task_id, uint64_t p_task_id);
    uint64_t GetPID(uint64_t task_id);

   private:
    std::vector<std::unique_ptr<AppInfo>> apps_{};
};

enum State {
    Error,
    InitialState,
    ExecuteFile,
    StartAppTask,
    Write,
    Open,
};

class ApplicationManagementServer {
   public:
    ApplicationManagementServer();
    void Initilize();
    void Processing();

   private:
    Message send_message_;
    Message received_message_;

    AppManager* app_manager_;

    State state_;

    uint64_t target_p_id_;
    uint64_t target_id_;
    char argument[32];
    uint64_t fs_id_;

    void ChangeState(State state) { state_ = state; }
    void ReceiveMessage();
};

ApplicationManagementServer* application_management_server;