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
#include "appmanager.hpp"
#include "filedescriptor.hpp"
#include "serverstate.hpp"

class ApplicationManagementServer {
   public:
    ApplicationManagementServer();
    void Initialize();

    void ReceiveMessage() { state_ = state_->ReceiveMessage(); }
    void HandleMessage() { state_ = state_->HandleMessage(); }
    void SendMessage() { state_ = state_->SendMessage(); }

   private:
    ServerState* GetServerState(State state) { return state_pool_[state]; }

    Message sm_;
    Message rm_;

    std::vector<::ServerState*> state_pool_{};
    ServerState* state_ = nullptr;

    AppManager* app_manager_;

    uint64_t fs_id_;

    uint64_t target_id_;
    uint64_t new_id_;

    char command_[32];
    char argument_[32];
    bool redirect_;
    bool pipe_;
    bool piped_ = false;
    char redirect_filename_[32];

    uint64_t pipe_task_id_;
    std::shared_ptr<PipeFileDescriptor> pipe_fd_;

    uint64_t waiting_task_id_;

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
    friend WaitingKeyState;
};

extern ApplicationManagementServer* server;