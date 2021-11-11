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
    msg.arg.read.offset = read_offset_;

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
                read_offset_ += rmsg.arg.read.len;
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

void ApplicationManagementServer::ReceiveMessage() {
    switch (state_) {
        case State::InitialState: {
            SyscallOpenReceiveMessage(&received_message_, 1);
        } break;

        case State::Error:
            break;

        default: {
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                Print("[ am ] cannnot find file system server\n");
                ChangeState(State::Error);
                break;
            }
            fs_id_ = fs_id;

            while (1) {
                SyscallClosedReceiveMessage(&received_message_, 1, fs_id_);
                if (received_message_.type == Message::kError) {
                    if (received_message_.arg.error.retry) {
                        SyscallSendMessage(&send_message_, fs_id_);
                        continue;
                    } else {
                        Print("[ am ] error at fs server\n");
                        send_message_.type = Message::kError;
                        send_message_.arg.error.retry = false;
                        SyscallSendMessage(&send_message_, target_id_);
                        ChangeState(State::InitialState);
                    }
                } else {
                    break;
                }
            }

        } break;
    }
}

void ApplicationManagementServer::ProcessMessage() {
    switch (state_) {
        case State::InitialState: {
            // message from kernel
            if (received_message_.src_task == 1) {
                switch (received_message_.type) {
                    case Message::kExit: {
                        target_id_ =
                            app_manager_->GetPID(received_message_.arg.exit.id);
                        if (target_id_) {
                            send_message_.type = Message::kExit;
                            send_message_.arg.exit.result =
                                received_message_.arg.exit.result;
                            ChangeState(State::Exit);
                        }
                    } break;

                    default: {
                        Print("[ am ] unknown message type from kernel\n");
                        ChangeState(State::Error);
                    } break;
                }
                // message from others
            } else {
                switch (received_message_.type) {
                    case Message::kExecuteFile: {
                        target_id_ = received_message_.src_task;
                        strcpy(argument_,
                               received_message_.arg.executefile.arg);

                        send_message_.type = Message::kExecuteFile;
                        strcpy(send_message_.arg.executefile.filename,
                               received_message_.arg.executefile.filename);

                        Print("[ am ] execute application %s\n",
                              received_message_.arg.executefile.filename);

                        ChangeState(State::ExecuteFile);
                    } break;

                    case Message::kWrite: {
                        auto app_info = app_manager_->GetAppInfo(
                            received_message_.src_task);
                        app_info->Files()[received_message_.arg.write.fd]
                            ->Write(received_message_);
                        target_id_ = received_message_.src_task;

                        send_message_.type = Message::kReceived;
                        ChangeState(State::InitialState);
                    } break;

                    case Message::kOpen: {
                        target_id_ = received_message_.src_task;
                        if (strcmp(received_message_.arg.open.filename,
                                   "@stdin") == 0) {
                            send_message_.type = Message::kOpen;
                            send_message_.arg.open.fd = 0;
                            send_message_.arg.open.exist = true;

                            ChangeState(State::InitialState);
                        } else {
                            send_message_.type = Message::kOpen;
                            strcpy(send_message_.arg.open.filename,
                                   received_message_.arg.open.filename);
                            ChangeState(State::Open);
                        }
                    } break;

                    case Message::kRead: {
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
                            ChangeState(State::Read);
                        }
                    } break;

                    default:
                        break;
                }
            }
        } break;

        case State::ExecuteFile: {
            if (received_message_.type == Message::kExecuteFile) {
                if (!received_message_.arg.executefile.exist ||
                    received_message_.arg.executefile.isdirectory) {
                    send_message_.type = Message::kExecuteFile;
                    send_message_.arg.executefile.exist =
                        received_message_.arg.executefile.exist;
                    send_message_.arg.executefile.isdirectory =
                        received_message_.arg.executefile.isdirectory;
                    ChangeState(State::InitialState);
                } else {
                    // the file exists, and not a directory
                    auto [id, err] = SyscallCreateNewTask();
                    if (err) {
                        Print("[ am ] syscall error\n");
                        send_message_.type = Message::kError;
                        send_message_.arg.error.retry = false;
                        SyscallSendMessage(&send_message_, target_id_);
                        SyscallSendMessage(&send_message_, fs_id_);
                        ChangeState(State::Error);
                    } else {
                        send_message_.type = Message::kExecuteFile;
                        send_message_.arg.executefile.id = id;
                        new_task_id_ = id;
                        ChangeState(State::CreateTask);
                    }
                }
            } else {
                Print("[ am ] unknown message type from fs\n");
                ChangeState(State::Error);
            }
        } break;

        case State::CreateTask: {
            if (received_message_.type == Message::kReady) {
                SyscallSetArgument(new_task_id_, argument_);
                app_manager_->NewApp(new_task_id_, target_id_);
                send_message_.type = Message::kStartApp;
                send_message_.arg.starttask.id = new_task_id_;
                ChangeState(State::StartApp);
            } else {
                Print("[ am ] unknown message type from fs\n");
                ChangeState(State::Error);
            }
        } break;

        case State::Open: {
            if (received_message_.type == Message::kOpen) {
                if (!received_message_.arg.open.exist) {
                    send_message_.type = Message::kOpen;
                    send_message_.arg.open.exist =
                        received_message_.arg.open.exist;
                    ChangeState(State::InitialState);
                } else {
                    auto app_info = app_manager_->GetAppInfo(target_id_);
                    size_t fd = AllocateFD(app_info);
                    app_info->Files()[fd] = std::make_unique<FatFileDescriptor>(
                        target_id_, received_message_.arg.open.filename);
                    Print("[ am ] allocate file descriptor for %s\n",
                          received_message_.arg.open.filename);

                    send_message_ = received_message_;
                    send_message_.arg.open.fd = fd;
                    ChangeState(State::InitialState);
                }
            } else {
                Print("[ am ] unknown message type from fs\n");
                ChangeState(State::Error);
            }
        } break;

        default:
            break;
    }
}

void ApplicationManagementServer::SendMessage() {
    switch (state_) {
        case State::InitialState: {
            SyscallSendMessage(&send_message_, target_id_);
        } break;

        case State::Exit: {
            SyscallSendMessage(&send_message_, target_id_);
            send_message_.type = Message::kReceived;
            SyscallSendMessage(&send_message_, received_message_.arg.exit.id);
            ChangeState(State::InitialState);
        } break;

        case State::Open:
        case State::CreateTask:
        case State::ExecuteFile: {
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                send_message_.type = Message::kError;
                send_message_.arg.error.retry = false;
                Print("[ am ] cannnot find file system server\n");
                SyscallSendMessage(&send_message_, target_id_);
                ChangeState(State::InitialState);
            } else {
                fs_id_ = fs_id;

                SyscallSendMessage(&send_message_, fs_id_);
            }
        } break;

        case State::StartApp: {
            SyscallSendMessage(&send_message_, 1);
            ChangeState(State::InitialState);
        } break;

        case State::Read:
        case State::Error: {
            ChangeState(State::InitialState);
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
        application_management_server->ReceiveMessage();
        application_management_server->ProcessMessage();
        application_management_server->SendMessage();
    }
}
