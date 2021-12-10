#pragma once
#include <cstdio>
#include <memory>
#include <vector>

#include "filedescriptor.hpp"

enum AppState {
    Run,
    Kill,
};

class AppInfo {
   public:
    AppInfo(uint64_t task_id, uint64_t p_task_id);
    uint64_t ID() { return task_id_; }
    uint64_t PID() { return p_task_id_; }
    std::vector<std::shared_ptr<::FileDescriptor>>& Files();
    void SetState(AppState state) { state_ = state; }
    AppState GetState() { return state_; }

   private:
    uint64_t task_id_;
    uint64_t p_task_id_;
    AppState state_ = AppState::Run;
    std::vector<std::shared_ptr<::FileDescriptor>> files_{};
};

class AppManager {
   public:
    AppManager();
    void NewApp(uint64_t task_id, uint64_t p_task_id);
    uint64_t GetPID(uint64_t task_id);
    AppInfo* GetAppInfo(uint64_t task_id);
    void ExitApp(uint64_t task_id);
    size_t AllocateFD(uint64_t task_id);

   private:
    std::vector<std::unique_ptr<AppInfo>> apps_{};

    void InitializeFileDescriptor(uint64_t task_id);
    void CopyFileDescriptor(uint64_t task_id, uint64_t p_task_id);
};