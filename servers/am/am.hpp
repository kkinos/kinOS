#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "../../libs/common/message.hpp"
#include "../../libs/common/template.hpp"
#include "../../libs/kinos/common/syscall.h"
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

   private:
    std::vector<std::unique_ptr<AppInfo>> apps_{};

    void InitializeFileDescriptor(uint64_t task_id);
    void CopyFileDescriptor(uint64_t task_id, uint64_t p_task_id);
};

enum State {
    StateErr,
    StateInit,
    StateExecFile,
    StateCreateTask,
    StateStartTask,
    StateRedirect,
    StatePipe,
    StateExit,
    StateOpen,
    StateAllcateFD,
    StateRead,
    StateWrite,
};

enum Target {
    File,
    Dir,
};

class ApplicationManagementServer;

class ServerState {
   public:
    virtual ~ServerState() = default;
    virtual ServerState* ReceiveMessage() = 0;
    virtual ServerState* HandleMessage() = 0;
    virtual ServerState* SendMessage() = 0;
};

class ErrState : public ::ServerState {
   public:
    explicit ErrState(ApplicationManagementServer* server) { server_ = server; }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override { return this; }
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class InitState : public ::ServerState {
   public:
    explicit InitState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override { return this; }
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class ExecFileState : public ::ServerState {
   public:
    explicit ExecFileState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class CreateTaskState : public ::ServerState {
   public:
    explicit CreateTaskState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class StartTaskState : public ::ServerState {
   public:
    explicit StartTaskState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class RedirectState : public ::ServerState {
   public:
    explicit RedirectState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class PipeState : public ::ServerState {
   public:
    explicit PipeState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override { return this; }
    ServerState* SendMessage() override { return this; }

   private:
    ApplicationManagementServer* server_;
};

class ExitState : public ::ServerState {
   public:
    explicit ExitState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class OpenState : public ::ServerState {
   public:
    explicit OpenState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

    void SetTarget(Target target) { target_ = target; }

   private:
    ApplicationManagementServer* server_;
    Target target_;
};

class AllocateFDState : public ::ServerState {
   public:
    explicit AllocateFDState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override { return this; }

    void SetTarget(Target target) { target_ = target; }

   private:
    ApplicationManagementServer* server_;
    Target target_;
};

class ReadState : public ::ServerState {
   public:
    explicit ReadState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

    void SetTarget(Target target) { target_ = target; }

   private:
    ApplicationManagementServer* server_;
    Target target_;
};

class WriteState : public ::ServerState {
   public:
    explicit WriteState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class ApplicationManagementServer {
   public:
    ApplicationManagementServer();
    void Initialize();

    void ReceiveMessage() { state_ = state_->ReceiveMessage(); }
    void HandleMessage() { state_ = state_->HandleMessage(); }
    void SendMessage() { state_ = state_->SendMessage(); }

   private:
    ServerState* GetServerState(State state) { return state_pool_[state]; }

    Message send_message_;
    Message received_message_;

    std::vector<::ServerState*> state_pool_{};
    ServerState* state_ = nullptr;

    AppManager* app_manager_;

    uint64_t target_id_;
    uint64_t new_task_id_;

    uint64_t pipe_task_id_;

    char task_command_[32];
    char task_argument_[32];
    bool redirect_;
    bool pipe_;
    bool piped_ = false;
    char redirect_filename_[32];

    std::shared_ptr<PipeFileDescriptor> pipe_fd_;

    uint64_t fs_id_;
    size_t AllocateFD(AppInfo* app_info);

    friend ErrState;
    friend InitState;
    friend ExecFileState;
    friend CreateTaskState;
    friend StartTaskState;
    friend RedirectState;
    friend PipeState;
    friend ExitState;
    friend OpenState;
    friend AllocateFDState;
    friend ReadState;
    friend WriteState;
};

ApplicationManagementServer* server;