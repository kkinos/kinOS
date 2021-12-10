#include "serverstate.hpp"

#include <errno.h>
#include <fcntl.h>

#include <cstdio>

#include "../../libs/kinos/common/print.hpp"
#include "am.hpp"

ServerState* ErrState::SendMessage() {
    return server_->GetServerState(State::StateInit);
};

ServerState* InitState::ReceiveMessage() {
    SyscallOpenReceiveMessage(&server_->rm_, 1);

    auto [fs_id, err] = SyscallFindServer("servers/fs");
    if (err) {
        server_->sm_.type = Message::kError;
        server_->sm_.arg.error.retry = false;
        server_->sm_.arg.error.err = EAGAIN;
        Print("[ am ] cannnot find file system server\n");
        return server_->GetServerState(State::StateErr);
    }

    server_->fs_id_ = fs_id;

    // message from others
    switch (server_->rm_.type) {
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

        case Message::kWaitingKey: {
            return server_->GetServerState(State::StateWaitingKey);
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
    SyscallSendMessage(&server_->sm_, server_->target_id_);
    return this;
}

ServerState* ExecFileState::HandleMessage() {
    server_->target_id_ = server_->rm_.src_task;

    strcpy(server_->command_, server_->rm_.arg.executefile.filename);
    strcpy(server_->argument_, server_->rm_.arg.executefile.arg);

    if (server_->rm_.arg.executefile.pipe) {
        server_->pipe_ = true;
    } else {
        server_->pipe_ = false;
    }

    if (server_->rm_.arg.executefile.redirect) {
        server_->redirect_ = true;
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->target_id_);
        strcpy(server_->redirect_filename_, server_->rm_.arg.redirect.filename);
    } else {
        server_->redirect_ = false;
    }

    server_->sm_.type = Message::kExecuteFile;
    strcpy(server_->sm_.arg.executefile.filename, server_->command_);

    Print("[ am ] execute %s\n", server_->command_);
    return this;
}

ServerState* ExecFileState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->fs_id_);
    return this;
}

ServerState* ExecFileState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->fs_id_);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = server_->rm_.arg.error.err;
                    SyscallSendMessage(&server_->sm_, server_->target_id_);
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
        server_->sm_.type = Message::kError;
        server_->sm_.arg.error.retry = false;
        server_->sm_.arg.error.err = EAGAIN;
        SyscallSendMessage(&server_->sm_, server_->target_id_);
        SyscallSendMessage(&server_->sm_, server_->fs_id_);
        return server_->GetServerState(State::StateErr);
    } else {
        server_->sm_.type = Message::kExecuteFile;
        server_->sm_.arg.executefile.id = id;
        server_->new_id_ = id;
        return this;
    }
}

ServerState* CreateTaskState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->fs_id_);
    return this;
}

ServerState* CreateTaskState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->fs_id_);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = EAGAIN;
                    SyscallSendMessage(&server_->sm_, server_->target_id_);
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
    server_->app_manager_->NewApp(server_->new_id_, server_->target_id_);

    auto app_info = server_->app_manager_->GetAppInfo(server_->new_id_);

    if (server_->redirect_) {
        app_info->Files()[1] = std::make_unique<FatFileDescriptor>(
            server_->new_id_, server_->redirect_filename_);
    }

    if (server_->piped_) {
        app_info->Files()[0] = server_->pipe_fd_;
        server_->piped_ = false;
    }

    if (server_->pipe_) {
        server_->pipe_fd_ =
            std::make_unique<PipeFileDescriptor>(server_->new_id_);
        app_info->Files()[1] = server_->pipe_fd_;
    }

    SyscallSetCommand(server_->new_id_, server_->command_);
    SyscallSetArgument(server_->new_id_, server_->argument_);

    server_->sm_.type = Message::kExecuteFile;
    server_->sm_.arg.executefile.id = server_->new_id_;
    SyscallSendMessage(&server_->sm_, server_->target_id_);

    server_->sm_.type = Message::kStartApp;
    server_->sm_.arg.starttask.id = server_->new_id_;
    return this;
}

ServerState* StartTaskState::SendMessage() {
    SyscallSendMessage(&server_->sm_, 1);
    if (server_->pipe_) {
        server_->sm_.type = Message::kReady;
        SyscallSendMessage(&server_->sm_, server_->target_id_);
        return server->GetServerState(State::StatePipe);
    }
    return server_->GetServerState(State::StateInit);
}

ServerState* RedirectState::HandleMessage() {
    server_->sm_.type = Message::kOpen;
    strcpy(server_->sm_.arg.open.filename, server_->redirect_filename_);
    server_->sm_.arg.open.flags = O_CREAT;
    return this;
}

ServerState* RedirectState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->fs_id_);
    return this;
}

ServerState* RedirectState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->fs_id_);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = server_->rm_.arg.error.err;
                    SyscallSendMessage(&server_->sm_, server_->target_id_);
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
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->target_id_);
        switch (server_->rm_.type) {
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
    server_->target_id_ =
        server_->app_manager_->GetPID(server_->rm_.arg.exitapp.id);
    if (server_->target_id_) {
        server_->app_manager_->ExitApp(server_->rm_.arg.exitapp.id);
        server_->sm_.type = Message::kExitApp;
        server_->sm_.arg.exitapp.result = server_->rm_.arg.exitapp.result;
        server_->sm_.arg.exitapp.id = server_->rm_.arg.exitapp.id;
        return this;
    }
    return server_->GetServerState(State::StateErr);
}

ServerState* ExitState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->target_id_);

    server_->sm_.type = Message::kReceived;
    SyscallSendMessage(&server_->sm_, server_->rm_.arg.exitapp.id);
    return server_->GetServerState(State::StateInit);
}

ServerState* OpenState::HandleMessage() {
    server_->target_id_ = server_->rm_.src_task;
    if (server_->rm_.type == Message::kOpen) {
        SetTarget(Target::File);
    } else if (server_->rm_.type == Message::kOpenDir) {
        SetTarget(Target::Dir);
    }
    switch (target_) {
        case Target::File: {
            if (strcmp(server_->rm_.arg.open.filename, "@stdin") == 0) {
                server_->sm_.type = Message::kOpen;
                server_->sm_.arg.open.fd = 0;
                server_->sm_.arg.open.exist = true;
                server_->sm_.arg.open.isdirectory = false;
                return server_->GetServerState(State::StateInit);
            } else {
                server_->sm_.type = Message::kOpen;
                strcpy(server_->sm_.arg.open.filename,
                       server_->rm_.arg.open.filename);
                server_->sm_.arg.open.flags = server_->rm_.arg.open.flags;
                return this;
            }

        } break;

        case Target::Dir: {
            server_->sm_.type = Message::kOpenDir;
            strcpy(server_->sm_.arg.opendir.dirname,
                   server_->rm_.arg.opendir.dirname);
            return this;
        } break;

        default:
            break;
    }
}

ServerState* OpenState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->fs_id_);
    return this;
}

ServerState* OpenState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->fs_id_);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, server_->fs_id_);
                    continue;
                } else {
                    Print("[ am ] error at fs server\n");
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = server_->rm_.arg.error.err;
                    SyscallSendMessage(&server_->sm_, server_->target_id_);
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
    if (server_->rm_.type == Message::kOpen) {
        SetTarget(Target::File);
    } else if (server_->rm_.type == Message::kOpenDir) {
        SetTarget(Target::Dir);
    }

    switch (target_) {
        case Target::File: {
            server_->sm_.type = Message::kOpen;
            size_t fd = server_->app_manager_->AllocateFD(server_->target_id_);
            auto app_info =
                server_->app_manager_->GetAppInfo(server_->target_id_);
            app_info->Files()[fd] = std::make_unique<FatFileDescriptor>(
                server_->target_id_, server_->rm_.arg.open.filename);
            server_->sm_.arg.open.fd = fd;
            Print("[ am ] allocate file descriptor for %s\n",
                  server_->rm_.arg.open.filename);
            return server_->GetServerState(State::StateInit);
        } break;

        case Target::Dir: {
            server_->sm_.type = Message::kOpenDir;
            size_t fd = server_->app_manager_->AllocateFD(server_->target_id_);
            auto app_info =
                server_->app_manager_->GetAppInfo(server_->target_id_);
            app_info->Files()[fd] = std::make_unique<FatFileDescriptor>(
                server_->target_id_, server_->rm_.arg.opendir.dirname);
            server_->sm_.arg.opendir.fd = fd;
            Print("[ am ] allocate file descriptor for %s\n",
                  server_->rm_.arg.opendir.dirname);
            return server_->GetServerState(State::StateInit);

        } break;

        default:
            break;
    }
}

ServerState* ReadState::HandleMessage() {
    server_->target_id_ = server_->rm_.src_task;

    size_t fd = server_->rm_.arg.read.fd;
    auto app_info = server_->app_manager_->GetAppInfo(server_->target_id_);

    if (fd < 0 || app_info->Files().size() <= fd || !app_info->Files()[fd]) {
        server_->sm_.type = Message::kError;
        server_->sm_.arg.error.retry = false;
        server_->sm_.arg.error.err = EBADF;
        Print("[ am ] %d bad file number\n", fd);
        return server_->GetServerState(State::StateInit);

    } else {
        app_info->Files()[fd]->Read(server_->rm_);
        return this;
    }
}

ServerState* ReadState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}

ServerState* WriteState::HandleMessage() {
    server_->target_id_ = server_->rm_.src_task;

    size_t fd = server_->rm_.arg.write.fd;
    auto app_info = server_->app_manager_->GetAppInfo(server_->target_id_);

    if (fd < 0 || app_info->Files().size() <= fd || !app_info->Files()[fd]) {
        server_->sm_.type = Message::kError;
        server_->sm_.arg.error.retry = false;
        server_->sm_.arg.error.err = EBADF;
        Print("[ am ] %d bad file number\n", fd);
        return server_->GetServerState(State::StateInit);
    } else {
        app_info->Files()[fd]->Write(server_->rm_);
        return this;
    }
}

ServerState* WriteState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}

ServerState* WaitingKeyState::HandleMessage() {
    if (waiting_) {
        server_->sm_ = server_->rm_;
        waiting_ = false;
        return this;
    } else {
        waiting_id_ = server_->rm_.src_task;
        server_->sm_.type = Message::kReceived;
        SyscallSendMessage(&server_->sm_, waiting_id_);

        auto app_info = server_->app_manager_->GetAppInfo(waiting_id_);
        uint64_t pid = app_info->PID();

        server_->target_id_ = pid;
        server_->sm_.type = Message::kWaitingKey;
        waiting_ = true;
        return this;
    }
}

ServerState* WaitingKeyState::SendMessage() {
    if (waiting_) {
        SyscallSendMessage(&server_->sm_, server_->target_id_);
        return this;
    } else {
        SyscallSendMessage(&server_->sm_, waiting_id_);
        return server_->GetServerState(State::StateInit);
    }
}

ServerState* WaitingKeyState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->target_id_);
        switch (server_->rm_.type) {
            case Message::kKeyPush:
            case Message::kQuit:
                return this;
                break;

            default: {
                Message m;
                m.type = Message::kError;
                m.arg.error.retry = false;
                SyscallSendMessage(&m, server_->target_id_);
                return server_->GetServerState(State::StateErr);
            } break;
        }
    }
}