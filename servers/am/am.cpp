#include "am.hpp"

#include <errno.h>

#include <cstdio>

#include "../../libs/kinos/common/print.hpp"

extern "C" void main() {
    server = new ApplicationManagementServer;
    server->Initialize();

    while (true) {
        server->ReceiveMessage();
        server->HandleMessage();
        server->SendMessage();
    }
}

ErrState::ErrState(ApplicationManagementServer* server) : server_{server} {}

ServerState* ErrState::SendMessage() {
    return server_->GetServerState(State::StateInit);
};

InitState::InitState(ApplicationManagementServer* server) : server_{server} {}

ServerState* InitState::ReceiveMessage() {
    SyscallOpenReceiveMessage(&server_->received_message_, 1);
    if (server_->received_message_.src_task == 1) {
        // message from kernel
        switch (server_->received_message_.type) {
            case Message::kExitApp: {
                return server_->GetServerState(State::StateExit);
            } break;

            default: {
                Print("[ am ] unknown message type from kernel");
                return server_->GetServerState(State::StateErr);
            } break;
        }
    } else {
        // message from others
        switch (server_->received_message_.type) {
            case Message::kExecuteFile: {
                return server_->GetServerState(State::StateExecFile);
            } break;
            case Message::kWrite: {
                return server_->GetServerState(State::StateWrite);
            } break;
            case Message::kOpen:
            case Message::kOpenDir: {
                return server_->GetServerState(State::StateOpen);
            } break;

            case Message::kRead: {
                return server_->GetServerState(State::StateRead);
            } break;
            default:
                Print("[ am ] unknown message type\n");
                return this;
                break;
        }
    }
}

ServerState* InitState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->target_id_);
    return this;
}

ExecFileState::ExecFileState(ApplicationManagementServer* server)
    : server_{server} {}

ServerState* ExecFileState::ReceiveMessage() {
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        Print("[ am ] cannnot find file system server\n");
        return server_->GetServerState(State::StateErr);
    }
    server_->fs_id_ = fs_id;

    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1,
                                    server_->fs_id_);
        switch (server_->received_message_.type) {
            case Message::kError: {
                if (server_->received_message_.arg.error.retry) {
                    SyscallSendMessage(&server_->send_message_,
                                       server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->send_message_.type = Message::kError;
                    server_->send_message_.arg.error.retry = false;
                    server_->send_message_.arg.error.err =
                        server_->received_message_.arg.error.err;
                    SyscallSendMessage(&server_->send_message_,
                                       server_->target_id_);
                    return server_->GetServerState(State::StateErr);
                }
            } break;

            case Message::kExecuteFile: {
                return server_->GetServerState(State::StateCreateTask);
            } break;

            default:
                Print("[ am ] unknown message from fs\n");
                return server_->GetServerState(State::StateErr);
        }
    }
}

ServerState* ExecFileState::HandleMessage() {
    server_->target_id_ = server_->received_message_.src_task;
    strcpy(server_->task_argument_,
           server_->received_message_.arg.executefile.arg);
    strcpy(server_->task_command_,
           server_->received_message_.arg.executefile.filename);

    server_->send_message_.type = Message::kExecuteFile;
    strcpy(server_->send_message_.arg.executefile.filename,
           server_->received_message_.arg.executefile.filename);

    Print("[ am ] execute application %s\n",
          server_->received_message_.arg.executefile.filename);

    return this;
}

ServerState* ExecFileState::SendMessage() {
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EAGAIN;
        Print("[ am ] cannnot find file system server\n");
        SyscallSendMessage(&server_->send_message_, server_->target_id_);
        return server_->GetServerState(State::StateInit);
    } else {
        server_->fs_id_ = fs_id;

        SyscallSendMessage(&server_->send_message_, server_->fs_id_);
        return this;
    }
}

CreateTaskState::CreateTaskState(ApplicationManagementServer* server)
    : server_{server} {}

ServerState* CreateTaskState::ReceiveMessage() {
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EAGAIN;
        Print("[ am ] cannnot find file system server\n");
        SyscallSendMessage(&server_->send_message_, server_->target_id_);
        return server_->GetServerState(State::StateErr);
    }
    server_->fs_id_ = fs_id;

    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1,
                                    server_->fs_id_);
        switch (server_->received_message_.type) {
            case Message::kError: {
                if (server_->received_message_.arg.error.retry) {
                    SyscallSendMessage(&server_->send_message_,
                                       server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->send_message_.type = Message::kError;
                    server_->send_message_.arg.error.retry = false;
                    server_->send_message_.arg.error.err = EAGAIN;
                    SyscallSendMessage(&server_->send_message_,
                                       server_->target_id_);
                    return server_->GetServerState(State::StateErr);
                }
            } break;

            case Message::kReady: {
                return server_->GetServerState(State::StateStartTask);
            } break;

            default: {
                Print("[ am ] unknown message from fs\n");
                return server_->GetServerState(State::StateErr);
                break;
            }
        }
    }
}

ServerState* CreateTaskState::HandleMessage() {
    auto [id, err] = SyscallCreateNewTask();
    if (err) {
        Print("[ am ] syscall error\n");
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EAGAIN;
        SyscallSendMessage(&server_->send_message_, server_->target_id_);
        SyscallSendMessage(&server_->send_message_, server_->fs_id_);
        return server_->GetServerState(State::StateErr);
    } else {
        server_->send_message_.type = Message::kExecuteFile;
        server_->send_message_.arg.executefile.id = id;
        server_->new_task_id_ = id;
        return this;
    }
}

ServerState* CreateTaskState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->fs_id_);
    return this;
}

StartTaskState::StartTaskState(ApplicationManagementServer* server)
    : server_{server} {}

ServerState* StartTaskState::HandleMessage() {
    SyscallSetCommand(server_->new_task_id_, server_->task_command_);
    SyscallSetArgument(server_->new_task_id_, server_->task_argument_);
    server_->app_manager_->NewApp(server_->new_task_id_, server_->target_id_);
    server_->send_message_.type = Message::kStartApp;
    server_->send_message_.arg.starttask.id = server_->new_task_id_;
    return this;
}

ServerState* StartTaskState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, 1);
    return server_->GetServerState(State::StateInit);
}

ExitState::ExitState(ApplicationManagementServer* server) : server_{server} {}

ServerState* ExitState::HandleMessage() {
    server_->target_id_ = server_->app_manager_->GetPID(
        server_->received_message_.arg.exitapp.id);
    if (server_->target_id_) {
        server_->app_manager_->ExitApp(
            server_->received_message_.arg.exitapp.id);
        server_->send_message_.type = Message::kExitApp;
        server_->send_message_.arg.exitapp.result =
            server_->received_message_.arg.exitapp.result;
        return this;
    }
}

ServerState* ExitState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->target_id_);
    server_->send_message_.type = Message::kReceived;
    SyscallSendMessage(&server_->send_message_,
                       server_->received_message_.arg.exitapp.id);
    return server_->GetServerState(State::StateInit);
}

OpenState::OpenState(ApplicationManagementServer* server) : server_{server} {}

ServerState* OpenState::ReceiveMessage() {
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        Print("[ am ] cannnot find file system server\n");
        return server_->GetServerState(State::StateErr);
    }
    server_->fs_id_ = fs_id;

    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1,
                                    server_->fs_id_);
        switch (server_->received_message_.type) {
            case Message::kError: {
                if (server_->received_message_.arg.error.retry) {
                    SyscallSendMessage(&server_->send_message_,
                                       server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->send_message_.type = Message::kError;
                    server_->send_message_.arg.error.retry = false;
                    server_->send_message_.arg.error.err =
                        server_->received_message_.arg.error.err;
                    SyscallSendMessage(&server_->send_message_,
                                       server_->target_id_);
                    return server_->GetServerState(State::StateErr);
                }
            } break;

            case Message::kOpen:
            case Message::kOpenDir: {
                return server_->GetServerState(State::StateAllcateFD);
            } break;

            default:
                Print("[ am ] unknown message from fs\n");
                return server_->GetServerState(State::StateErr);
                break;
        }
    }
}

ServerState* OpenState::HandleMessage() {
    server_->target_id_ = server_->received_message_.src_task;
    if (server_->received_message_.type == Message::kOpen) {
        SetTarget(Target::File);
    } else if (server_->received_message_.type == Message::kOpenDir) {
        SetTarget(Target::Dir);
    }
    switch (target_) {
        case Target::File: {
            if (strcmp(server_->received_message_.arg.open.filename,
                       "@stdin") == 0) {
                server_->send_message_.type = Message::kOpen;
                server_->send_message_.arg.open.fd = 0;
                server_->send_message_.arg.open.exist = true;
                server_->send_message_.arg.open.isdirectory = false;
                return server_->GetServerState(State::StateInit);
            } else {
                server_->send_message_.type = Message::kOpen;
                strcpy(server_->send_message_.arg.open.filename,
                       server_->received_message_.arg.open.filename);
                server_->send_message_.arg.open.flags =
                    server_->received_message_.arg.open.flags;
                return this;
            }

        } break;

        case Target::Dir: {
            server_->send_message_.type = Message::kOpenDir;
            strcpy(server_->send_message_.arg.opendir.dirname,
                   server_->received_message_.arg.opendir.dirname);
            return this;
        } break;

        default:
            break;
    }
}

ServerState* OpenState::SendMessage() {
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EAGAIN;
        Print("[ am ] cannnot find file system server\n");
        SyscallSendMessage(&server_->send_message_, server_->target_id_);
        return server_->GetServerState(State::StateInit);
    } else {
        server_->fs_id_ = fs_id;
        SyscallSendMessage(&server_->send_message_, server_->fs_id_);
        return this;
    }
}

AllocateFDState::AllocateFDState(ApplicationManagementServer* server)
    : server_{server} {}

ServerState* AllocateFDState::HandleMessage() {
    if (server_->received_message_.type == Message::kOpen) {
        SetTarget(Target::File);
    } else if (server_->received_message_.type == Message::kOpenDir) {
        SetTarget(Target::Dir);
    }

    switch (target_) {
        case Target::File: {
            server_->send_message_.type = Message::kOpen;
            auto app_info =
                server_->app_manager_->GetAppInfo(server_->target_id_);
            size_t fd = server_->AllocateFD(app_info);
            app_info->Files()[fd] = std::make_unique<FatFileDescriptor>(
                server_->target_id_,
                server_->received_message_.arg.open.filename);
            server_->send_message_.arg.open.fd = fd;
            Print("[ am ] allocate file descriptor for %s\n",
                  server_->received_message_.arg.open.filename);
            return server_->GetServerState(State::StateInit);
        } break;

        case Target::Dir: {
            auto app_info =
                server_->app_manager_->GetAppInfo(server_->target_id_);
            size_t fd = server_->AllocateFD(app_info);
            app_info->Files()[fd] = std::make_unique<FatFileDescriptor>(
                server_->target_id_,
                server_->received_message_.arg.opendir.dirname);
            server_->send_message_.arg.opendir.fd = fd;
            Print("[ am ] allocate file descriptor for %s\n",
                  server_->received_message_.arg.opendir.dirname);
            return server_->GetServerState(State::StateInit);

        } break;

        default:
            break;
    }
}

ReadState::ReadState(ApplicationManagementServer* server) : server_{server} {}

ServerState* ReadState::HandleMessage() {
    server_->target_id_ = server_->received_message_.src_task;

    size_t fd = server_->received_message_.arg.read.fd;
    auto app_info = server_->app_manager_->GetAppInfo(server_->target_id_);

    if (fd < 0 || app_info->Files().size() <= fd || !app_info->Files()[fd]) {
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EBADF;
        Print("[ am ] %d bad file number\n", fd);
        return server_->GetServerState(State::StateInit);

    } else {
        app_info->Files()[fd]->Read(server_->received_message_);
        return this;
    }
}

ServerState* ReadState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}

WriteState::WriteState(ApplicationManagementServer* server) : server_{server} {}

ServerState* WriteState::HandleMessage() {
    server_->target_id_ = server_->received_message_.src_task;

    size_t fd = server_->received_message_.arg.write.fd;
    auto app_info = server_->app_manager_->GetAppInfo(server_->target_id_);

    if (fd < 0 || app_info->Files().size() <= fd || !app_info->Files()[fd]) {
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EBADF;
        Print("[ am ] %d bad file number\n", fd);
        return server_->GetServerState(State::StateInit);
    } else {
        app_info->Files()[fd]->Write(server_->received_message_);
        return this;
    }
}

ServerState* WriteState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}

ApplicationManagementServer::ApplicationManagementServer() {}

void ApplicationManagementServer::Initialize() {
    app_manager_ = new AppManager;

    state_pool_.emplace_back(new ErrState(this));
    state_pool_.emplace_back(new InitState(this));
    state_pool_.emplace_back(new ExecFileState(this));
    state_pool_.emplace_back(new CreateTaskState(this));
    state_pool_.emplace_back(new StartTaskState(this));
    state_pool_.emplace_back(new ExitState(this));
    state_pool_.emplace_back(new OpenState(this));
    state_pool_.emplace_back(new AllocateFDState(this));
    state_pool_.emplace_back(new ReadState(this));
    state_pool_.emplace_back(new WriteState(this));

    state_ = GetServerState(State::StateInit);

    Print("[ am ] ready\n");
}

size_t ApplicationManagementServer::AllocateFD(AppInfo* app_info) {
    const size_t num_files = app_info->Files().size();
    for (size_t i = 0; i < num_files; ++i) {
        if (!app_info->Files()[i]) {
            return i;
        }
    }
    app_info->Files().emplace_back();
    return num_files;
}

AppInfo::AppInfo(uint64_t task_id, uint64_t p_task_id)
    : task_id_{task_id}, p_task_id_{p_task_id} {}

std::vector<std::shared_ptr<::FileDescriptor>>& AppInfo::Files() {
    return files_;
}

AppManager::AppManager() {}

void AppManager::NewApp(uint64_t task_id, uint64_t p_task_id) {
    auto it = std::find_if(
        apps_.begin(), apps_.end(),
        [p_task_id](const auto& t) { return t->ID() == p_task_id; });

    if (it == apps_.end()) {
        // dont have parent
        apps_.emplace_back(new AppInfo{task_id, p_task_id});
        InitializeFileDescriptor(task_id);

    } else {
        apps_.emplace_back(new AppInfo{task_id, p_task_id});
        CopyFileDescriptor(task_id, p_task_id);
    }
}

uint64_t AppManager::GetPID(uint64_t task_id) {
    auto it =
        std::find_if(apps_.begin(), apps_.end(),
                     [task_id](const auto& t) { return t->ID() == task_id; });
    if (it == apps_.end()) {
        return 0;
    }
    return (*it)->PID();
}

AppInfo* AppManager::GetAppInfo(uint64_t task_id) {
    auto it =
        std::find_if(apps_.begin(), apps_.end(),
                     [task_id](const auto& t) { return t->ID() == task_id; });
    if (it == apps_.end()) {
        return nullptr;
    }
    return it->get();
}

void AppManager::ExitApp(uint64_t task_id) {
    while (1) {
        auto it =
            std::find_if(apps_.begin(), apps_.end(), [task_id](const auto& t) {
                return t->ID() == task_id && t->GetState() == AppState::Run;
            });
        if (it == apps_.end()) {
            break;
        }
        (*it)->Files().clear();
        (*it)->SetState(AppState::Kill);
    }
}

void AppManager::InitializeFileDescriptor(uint64_t task_id) {
    auto it =
        std::find_if(apps_.begin(), apps_.end(),
                     [task_id](const auto& t) { return t->ID() == task_id; });
    if (it == apps_.end()) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        (*it)->Files().emplace_back(new TerminalFileDescriptor(task_id));
    }
}

void AppManager::CopyFileDescriptor(uint64_t task_id, uint64_t p_task_id) {
    auto child =
        std::find_if(apps_.begin(), apps_.end(),
                     [task_id](const auto& t) { return t->ID() == task_id; });
    auto parent = std::find_if(
        apps_.begin(), apps_.end(),
        [p_task_id](const auto& t) { return t->ID() == p_task_id; });

    if (child == apps_.end() || parent == apps_.end()) {
        return;
    }

    for (int i = 0; i < (*parent)->Files().size(); i++) {
        (*child)->Files().emplace_back();
    }
    for (int i = 0; i < (*parent)->Files().size(); i++) {
        (*child)->Files()[i] = (*parent)->Files()[i];
    }
}

TerminalFileDescriptor::TerminalFileDescriptor(uint64_t id) : id_{id} {
    auto [t_id, err] = SyscallFindServer("servers/terminal");
    terminal_server_id_ = t_id;
}

size_t TerminalFileDescriptor::Write(Message msg) {
    if (msg.arg.write.len != 0) {
        SyscallSendMessage(&msg, terminal_server_id_);

        Message smsg;
        smsg.type = Message::kReceived;
        SyscallSendMessage(&smsg, id_);
    }
    return 0;
}
size_t TerminalFileDescriptor::Read(Message msg) {
    SyscallSendMessage(&msg, terminal_server_id_);
    Message rmsg;
    Message smsg;

    SyscallClosedReceiveMessage(&rmsg, 1, terminal_server_id_);
    SyscallSendMessage(&rmsg, id_);
    smsg.type = Message::kRead;
    smsg.arg.read.len = 0;
    SyscallSendMessage(&smsg, id_);
    return 0;
}

FatFileDescriptor::FatFileDescriptor(uint64_t id, char* filename) : id_{id} {
    strcpy(filename_, filename);
}

size_t FatFileDescriptor::Read(Message msg) {
    size_t count = msg.arg.read.count;
    msg.arg.read.offset = rd_off_;
    msg.arg.read.cluster = rd_cluster_;
    strcpy(msg.arg.read.filename, filename_);
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        msg.type = Message::kError;
        msg.arg.error.retry = false;
        Print("[ am ] cannnot find file system server\n");
        SyscallSendMessage(&msg, id_);
        return 0;
    } else {
        SyscallSendMessage(&msg, fs_id);
        Message rmsg;
        while (1) {
            SyscallClosedReceiveMessage(&rmsg, 1, fs_id);
            if (rmsg.arg.read.len != 0) {
                SyscallSendMessage(&rmsg, id_);
                rd_off_ += rmsg.arg.read.len;
                rd_cluster_ = rmsg.arg.read.cluster;

            } else {
                SyscallSendMessage(&rmsg, id_);
                break;
            }
        }
        return count;
    }
}

size_t FatFileDescriptor::Write(Message msg) {
    strcpy(msg.arg.write.filename, filename_);
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        msg.type = Message::kError;
        msg.arg.error.retry = false;
        Print("[ am ] cannnot find file system server\n");
        SyscallSendMessage(&msg, id_);
        return 0;
    } else {
        wr_off_ += msg.arg.write.len;
        msg.arg.write.offset = wr_off_;
        SyscallSendMessage(&msg, fs_id);

        Message smsg;
        smsg.type = Message::kReceived;
        SyscallSendMessage(&smsg, id_);
    }
}