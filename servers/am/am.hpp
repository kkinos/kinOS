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

Message sent_message[1];
Message received_message[1];

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

AppManager* app_manager;

void ProcessAccordingToMessage();

void CreateNewTask(uint64_t p_id, uint64_t fs_id, char* arg);

void ExecuteAppTask(uint64_t p_id, uint64_t id, char* arg, uint64_t fs_id);