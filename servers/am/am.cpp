#include "am.hpp"

#include <errno.h>
#include <fcntl.h>

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

ServerState* ErrState::SendMessage() {
    return server_->GetServerState(State::StateInit);
};

ServerState* InitState::ReceiveMessage() {
    SyscallOpenReceiveMessage(&server_->received_message_, 1);

    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        server_->send_message_.type = Message::kError;
        server_->send_message_.arg.error.retry = false;
        server_->send_message_.arg.error.err = EAGAIN;
        Print("[ am ] cannnot find file system server\n");
        return server_->GetServerState(State::StateErr);
    }

    server_->fs_id_ = fs_id;

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
        case Message::kExitApp: {
            return server_->GetServerState(State::StateExit);
        } break;

        default:
            Print("[ am ] unknown message type\n");
            break;
    }
}

ServerState* InitState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->target_id_);
    return this;
}

ServerState* ExecFileState::HandleMessage() {
    server_->target_id_ = server_->received_message_.src_task;

    strcpy(server_->task_command_,
           server_->received_message_.arg.executefile.filename);
    strcpy(server_->task_argument_,
           server_->received_message_.arg.executefile.arg);

    if (server_->received_message_.arg.executefile.pipe) {
        server_->pipe_ = true;
    } else {
        server_->pipe_ = false;
    }

    if (server_->received_message_.arg.executefile.redirect) {
        server_->redirect_ = true;
        SyscallClosedReceiveMessage(&server_->received_message_, 1,
                                    server_->target_id_);
        strcpy(server_->redirect_filename_,
               server_->received_message_.arg.redirect.filename);
    } else {
        server_->redirect_ = false;
    }

    server_->send_message_.type = Message::kExecuteFile;
    strcpy(server_->send_message_.arg.executefile.filename,
           server_->task_command_);

    Print("[ am ] execute %s\n", server_->task_command_);
    return this;
}

ServerState* ExecFileState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->fs_id_);
    return this;
}

ServerState* ExecFileState::ReceiveMessage() {
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
                break;
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

ServerState* CreateTaskState::ReceiveMessage() {
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
                if (server_->redirect_) {
                    return server_->GetServerState(State::StateRedirect);
                }
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

ServerState* StartTaskState::HandleMessage() {
    server_->app_manager_->NewApp(server_->new_task_id_, server_->target_id_);

    auto app_info = server_->app_manager_->GetAppInfo(server_->new_task_id_);

    if (server_->redirect_) {
        app_info->Files()[1] = std::make_unique<FatFileDescriptor>(
            server_->new_task_id_, server_->redirect_filename_);
    }

    if (server_->piped_) {
        app_info->Files()[0] = server_->pipe_fd_;
        server_->piped_ = false;
    }

    if (server_->pipe_) {
        server_->pipe_fd_ =
            std::make_unique<PipeFileDescriptor>(server_->new_task_id_);
        app_info->Files()[1] = server_->pipe_fd_;
    }

    SyscallSetCommand(server_->new_task_id_, server_->task_command_);
    SyscallSetArgument(server_->new_task_id_, server_->task_argument_);

    server_->send_message_.type = Message::kExecuteFile;
    server_->send_message_.arg.executefile.id = server_->new_task_id_;
    SyscallSendMessage(&server_->send_message_, server_->target_id_);

    server_->send_message_.type = Message::kStartApp;
    server_->send_message_.arg.starttask.id = server_->new_task_id_;
    return this;
}

ServerState* StartTaskState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, 1);
    if (server_->pipe_) {
        server_->send_message_.type = Message::kReady;
        SyscallSendMessage(&server_->send_message_, server_->target_id_);
        return server->GetServerState(State::StatePipe);
    }
    return server_->GetServerState(State::StateInit);
}

ServerState* RedirectState::HandleMessage() {
    server_->send_message_.type = Message::kOpen;
    strcpy(server_->send_message_.arg.open.filename,
           server_->redirect_filename_);
    server_->send_message_.arg.open.flags = O_CREAT;
    return this;
}

ServerState* RedirectState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->fs_id_);
    return this;
}

ServerState* RedirectState::ReceiveMessage() {
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

            case Message::kOpen: {
                return server_->GetServerState(State::StateStartTask);
            } break;

            default:
                Print("[ am ] unknown message from fs\n");
                return server_->GetServerState(State::StateErr);
                break;
        }
    }
}

ServerState* PipeState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1,
                                    server_->target_id_);
        switch (server_->received_message_.type) {
            case Message::kExecuteFile: {
                server_->piped_ = true;
                return server_->GetServerState(State::StateExecFile);
            } break;

            default:
                break;
        }
    }
}

ServerState* ExitState::HandleMessage() {
    server_->target_id_ = server_->app_manager_->GetPID(
        server_->received_message_.arg.exitapp.id);
    if (server_->target_id_) {
        server_->app_manager_->ExitApp(
            server_->received_message_.arg.exitapp.id);
        server_->send_message_.type = Message::kExitApp;
        server_->send_message_.arg.exitapp.result =
            server_->received_message_.arg.exitapp.result;
        server_->send_message_.arg.exitapp.id =
            server_->received_message_.arg.exitapp.id;
        return this;
    }
    return server_->GetServerState(State::StateErr);
}

ServerState* ExitState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->target_id_);

    server_->send_message_.type = Message::kReceived;
    SyscallSendMessage(&server_->send_message_,
                       server_->received_message_.arg.exitapp.id);
    return server_->GetServerState(State::StateInit);
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
    SyscallSendMessage(&server_->send_message_, server_->fs_id_);
    return this;
}

ServerState* OpenState::ReceiveMessage() {
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
            server_->send_message_.type = Message::kOpenDir;
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
    state_pool_.emplace_back(new RedirectState(this));
    state_pool_.emplace_back(new PipeState(this));
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
        } else {
            for (int i = 0; i < (*it)->Files().size(); ++i) {
                (*it)->Files()[i]->Close();
            }
            (*it)->Files().clear();
            (*it)->SetState(AppState::Kill);
        }
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
    SyscallSendMessage(&msg, terminal_server_id_);

    Message smsg;
    smsg.type = Message::kReceived;
    SyscallSendMessage(&smsg, id_);
    return 0;
}
size_t TerminalFileDescriptor::Read(Message msg) {
    SyscallSendMessage(&msg, terminal_server_id_);
    Message rmsg;
    Message smsg;

    SyscallClosedReceiveMessage(&rmsg, 1, terminal_server_id_);
    switch (rmsg.type) {
        case Message::kRead:
            SyscallSendMessage(&rmsg, id_);
            smsg.type = Message::kRead;
            smsg.arg.read.len = 0;
            SyscallSendMessage(&smsg, id_);
            break;

        default:
            Print("[ am ] unknown message from terminal\n");
            smsg.type = Message::kError;
            smsg.arg.error.retry = false;
            SyscallSendMessage(&smsg, terminal_server_id_);
            break;
    }
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
    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        msg.type = Message::kError;
        msg.arg.error.retry = false;
        Print("[ am ] cannnot find file system server\n");
        SyscallSendMessage(&msg, id_);
        return 0;
    } else {
        strcpy(msg.arg.write.filename, filename_);
        msg.arg.write.offset = wr_off_;
        SyscallSendMessage(&msg, fs_id);
        wr_off_ += msg.arg.write.len;

        Message smsg;
        smsg.type = Message::kReceived;
        SyscallSendMessage(&smsg, id_);
        while (1) {
            Message rmsg;
            SyscallClosedReceiveMessage(&rmsg, 1, id_);
            if (rmsg.type == Message::kWrite) {
                strcpy(rmsg.arg.write.filename, filename_);
                rmsg.arg.write.offset = wr_off_;
                SyscallSendMessage(&rmsg, fs_id);
                wr_off_ += rmsg.arg.write.len;

                smsg.type = Message::kReceived;
                SyscallSendMessage(&smsg, id_);

                if (rmsg.arg.write.len == 0) {
                    break;
                }
            }
        }
        return 0;
    }
}

PipeFileDescriptor::PipeFileDescriptor(uint64_t id) : id_{id} {}

size_t PipeFileDescriptor::Read(Message msg) {
    if (closed_) {
        Message smsg;
        smsg.type = Message::kRead;
        smsg.arg.read.len = 0;
        SyscallSendMessage(&smsg, msg.src_task);
        return 0;
    } else {
        size_t count = msg.arg.read.count;
        Message smsg;

        if (len_ == 0) {
            smsg.type = Message::kError;
            smsg.arg.error.retry = true;
            SyscallSendMessage(&smsg, msg.src_task);
            return 0;

        } else if (len_ <= count) {
            smsg.type = Message::kRead;
            memcpy(smsg.arg.read.data, data_, len_);
            smsg.arg.read.len = len_;
            len_ = 0;
            memset(data_, 0, sizeof(data_));
            SyscallSendMessage(&smsg, msg.src_task);

            smsg.type = Message::kRead;
            smsg.arg.read.len = 0;
            SyscallSendMessage(&smsg, msg.src_task);

            smsg.type = Message::kReceived;
            SyscallSendMessage(&smsg, id_);
            return 0;
        } else if (len_ > count) {
            smsg.type = Message::kRead;
            memcpy(smsg.arg.read.data, data_, count);
            smsg.arg.read.len = count;
            len_ -= count;
            SyscallSendMessage(&smsg, msg.src_task);
            smsg.arg.read.len = 0;
            SyscallSendMessage(&smsg, msg.src_task);

            memmove(data_, &data_[count], len_);
            return 0;
        }
    }
}

size_t PipeFileDescriptor::Write(Message msg) {
    if (closed_) {
        Message smsg;
        smsg.type = Message::kError;
        smsg.arg.error.retry = false;
        SyscallSendMessage(&smsg, id_);
        return 0;
    } else {
        if (len_ == 0) {
            if (msg.arg.write.len) {
                len_ = msg.arg.write.len;
                memcpy(data_, msg.arg.write.data, len_);
            } else {
                Message smsg;
                smsg.type = Message::kReceived;
                SyscallSendMessage(&smsg, id_);
            }
        } else {
            Message smsg;
            smsg.type = Message::kError;
            smsg.arg.error.retry = true;
            SyscallSendMessage(&smsg, id_);
        }
        return 0;
    }
}

void PipeFileDescriptor::Close() {
    closed_ = true;
    Message smsg;
    smsg.type = Message::kError;
    smsg.arg.error.retry = false;

    SyscallSendMessage(&smsg, id_);
}
