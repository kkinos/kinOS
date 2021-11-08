#include "am.hpp"

#include <cstdio>

#include "../../libs/kinos/print.hpp"

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
    return 0;
}
size_t TerminalFileDescriptor::Read(Message msg) {
    SyscallSendMessage(&msg, id_);
    return 0;
}

FatFileDescriptor::FatFileDescriptor(uint64_t id, char* filename) : id_{id} {
    strcpy(filename_, filename);
}

size_t FatFileDescriptor::Read(Message msg) {
    size_t count = msg.arg.read.count;
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
            if (rmsg.arg.read.len) {
                SyscallSendMessage(&rmsg, id_);
            } else {
                SyscallSendMessage(&rmsg, id_);
                break;
            }
        }
        return count;
    }
}

ApplicationManagementServer::ApplicationManagementServer() {}

void ApplicationManagementServer::Initilize() {
    app_manager_ = new AppManager;
    ChangeState(State::InitialState);
    Print("[ am ] ready\n");
}

void ApplicationManagementServer::Processing() {
    // TODO add case State::Error
    switch (state_) {
        case State::InitialState: {
            ReceiveMessage();
            // message from kernel
            if (received_message_.src_task == 1) {
                switch (received_message_.type) {
                    case Message::kExit: {
                        target_p_id_ =
                            app_manager_->GetPID(received_message_.arg.exit.id);
                        if (target_p_id_) {
                            send_message_.type = Message::kExit;
                            send_message_.arg.exit.result =
                                received_message_.arg.exit.result;
                            SyscallSendMessage(&send_message_, target_p_id_);

                            send_message_.type = Message::kReceived;
                            SyscallSendMessage(&send_message_,
                                               received_message_.arg.exit.id);
                        }
                    } break;

                    default:
                        Print("[ am ] unknown message type from kernel\n");
                        break;
                }
                // message from servers
            } else {
                switch (received_message_.type) {
                    case Message::kExecuteFile: {
                        ChangeState(State::ExecuteFile);
                    } break;

                    case Message::kWrite: {
                        ChangeState(State::Write);
                    } break;

                    case Message::kOpen: {
                        ChangeState(State::Open);
                    } break;

                    case Message::kRead: {
                        ChangeState(State::Read);
                    } break;

                    default:
                        break;
                }
            }
        } break;

        case State::ExecuteFile: {
            target_p_id_ = received_message_.src_task;
            strcpy(argument, received_message_.arg.executefile.arg);
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                send_message_.type = Message::kError;
                send_message_.arg.error.retry = false;
                Print("[ am ] cannnot find file system server\n");
                SyscallSendMessage(&send_message_, target_p_id_);
                ChangeState(State::InitialState);

            } else {
                fs_id_ = fs_id;
                send_message_.type = Message::kExecuteFile;
                strcpy(send_message_.arg.executefile.filename,
                       received_message_.arg.executefile.filename);
                Print("[ am ] execute application %s\n",
                      received_message_.arg.executefile.filename);

                SyscallSendMessage(&send_message_, fs_id_);

                ReceiveMessage();

                switch (received_message_.type) {
                    case Message::kExecuteFile: {
                        if (!received_message_.arg.executefile.exist ||
                            received_message_.arg.executefile.isdirectory) {
                            send_message_.type = Message::kExecuteFile;
                            send_message_.arg.executefile.exist =
                                received_message_.arg.executefile.exist;
                            send_message_.arg.executefile.isdirectory =
                                received_message_.arg.executefile.isdirectory;
                            SyscallSendMessage(&send_message_, target_p_id_);
                            ChangeState(State::InitialState);

                        } else {
                            // the file exists, and not a directory
                            auto [id, err] = SyscallCreateNewTask();
                            if (err) {
                                Print("[ am ] syscall error\n");
                                send_message_.type = Message::kError;
                                send_message_.arg.error.retry = false;
                                SyscallSendMessage(&send_message_,
                                                   target_p_id_);
                                SyscallSendMessage(&send_message_, fs_id_);
                                ChangeState(State::InitialState);
                            } else {
                                send_message_.type = Message::kExecuteFile;
                                send_message_.arg.executefile.id = id;
                                target_id_ = id;
                                SyscallSendMessage(&send_message_, fs_id_);
                                ChangeState(State::StartAppTask);
                            }
                        }
                    } break;

                    default:
                        Print("[ am ] unknown message type from fs server\n");
                        break;
                }
            }
        } break;

        case State::StartAppTask: {
            ReceiveMessage();
            switch (received_message_.type) {
                case Message::kReady: {
                    SyscallSetArgument(target_id_, argument);
                    app_manager_->NewApp(target_id_, target_p_id_);
                    send_message_.type = Message::kStartTask;
                    send_message_.arg.starttask.id = target_id_;
                    SyscallSendMessage(&send_message_, 1);
                    ChangeState(State::InitialState);
                } break;

                default:
                    Print("[ am ] unknown message type from fs server\n");
                    break;
            }
        } break;

        case State::Write: {
            auto app_info =
                app_manager_->GetAppInfo(received_message_.src_task);
            app_info->Files()[received_message_.arg.write.fd]->Write(
                received_message_);

            send_message_.type = Message::kReceived;
            SyscallSendMessage(&send_message_, received_message_.src_task);
            ChangeState(State::InitialState);
        } break;

        case State::Open: {
            // TODO: 1.flagの処理
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                send_message_.type = Message::kError;
                send_message_.arg.error.retry = false;
                Print("[ am ] cannnot find file system server\n");
                SyscallSendMessage(&send_message_, received_message_.src_task);
                ChangeState(State::InitialState);

            } else {
                fs_id_ = fs_id;
                target_id_ = received_message_.src_task;

                send_message_.type = Message::kOpen;
                strcpy(send_message_.arg.open.filename,
                       received_message_.arg.open.filename);
                SyscallSendMessage(&send_message_, fs_id_);

                ReceiveMessage();

                switch (received_message_.type) {
                    case Message::kOpen: {
                        if (!received_message_.arg.open.exist) {
                            send_message_.type = Message::kOpen;
                            send_message_.arg.open.exist =
                                received_message_.arg.open.exist;
                        } else {
                            auto app_info =
                                app_manager_->GetAppInfo(target_id_);
                            size_t fd = AllocateFD(app_info);

                            app_info->Files()[fd] =
                                std::make_unique<FatFileDescriptor>(
                                    target_id_,
                                    received_message_.arg.open.filename);
                            Print("[ am ] allocate file descriptor for %s\n",
                                  received_message_.arg.open.filename);

                            send_message_ = received_message_;
                            send_message_.arg.open.fd = fd;
                        }
                        SyscallSendMessage(&send_message_, target_id_);
                        ChangeState(State::InitialState);
                    } break;

                    default:
                        Print("[ am ] unknown message type from fs server\n");
                        break;
                }
            }
        } break;

        case State::Read: {
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                send_message_.type = Message::kError;
                send_message_.arg.error.retry = false;
                Print("[ am ] cannnot find file system server\n");
                SyscallSendMessage(&send_message_, received_message_.src_task);
                ChangeState(State::InitialState);
            } else {
                target_id_ = received_message_.src_task;
                size_t fd = received_message_.arg.read.fd;
                auto app_info = app_manager_->GetAppInfo(target_id_);

                if (fd < 0 || app_info->Files().size() <= fd ||
                    !app_info->Files()[fd]) {
                    send_message_.type = Message::kError;
                    send_message_.arg.error.retry = false;
                    Print("[ am ] %d bad file number\n", fd);
                    ChangeState(State::InitialState);
                } else {
                    app_info->Files()[fd]->Read(received_message_);
                    ChangeState(State::InitialState);
                }
            }
        } break;

        default:
            break;
    }
}

void ApplicationManagementServer::ReceiveMessage() {
    switch (state_) {
        case State::InitialState: {
            SyscallOpenReceiveMessage(&received_message_, 1);

        } break;

        case State::Open:
        case State::ExecuteFile:
        case State::StartAppTask: {
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                Print("[ am ] cannnot find file system server\n");
                ChangeState(State::Error);
                break;
            }
            fs_id_ = fs_id;

            SyscallClosedReceiveMessage(&received_message_, 1, fs_id_);
            if (received_message_.type == Message::kError) {
                if (received_message_.arg.error.retry) {
                    SyscallSendMessage(&send_message_, fs_id_);
                    break;
                } else {
                    Print("[ am ] error at fs server\n");
                    send_message_.type = Message::kError;
                    send_message_.arg.error.retry = false;
                    SyscallSendMessage(&send_message_, target_p_id_);
                    ChangeState(State::InitialState);
                }
            }
        } break;

        default:
            break;
    }
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

extern "C" void main() {
    application_management_server = new ApplicationManagementServer;
    application_management_server->Initilize();

    while (true) {
        application_management_server->Processing();
    }
}
