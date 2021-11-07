#include "am.hpp"

#include <cstdio>

#include "../../libs/kinos/print.hpp"

AppInfo::AppInfo(uint64_t task_id, uint64_t p_task_id)
    : task_id_{task_id}, p_task_id_{p_task_id} {}

AppManager::AppManager() {}

void AppManager::NewApp(uint64_t task_id, uint64_t p_task_id) {
    apps_.emplace_back(new AppInfo{task_id, p_task_id});
}

uint64_t AppManager::GetPID(uint64_t id) {
    auto it = std::find_if(apps_.begin(), apps_.end(),
                           [id](const auto& t) { return t->ID() == id; });
    if (it == apps_.end()) {
        return 0;
    }

    return (*it)->PID();
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
                        target_p_id_ = received_message_.src_task;
                        strcpy(argument, received_message_.arg.executefile.arg);
                        auto [fs_id, err] = SyscallFindServer("servers/fs");
                        if (err) {
                            send_message_.type = Message::kError;
                            send_message_.arg.error.retry = false;
                            Print("[ am ] cannnot find file system server\n");
                            SyscallSendMessage(&send_message_, target_p_id_);

                        } else {
                            fs_id_ = fs_id;
                            send_message_.type = Message::kExecuteFile;
                            strcpy(send_message_.arg.executefile.filename,
                                   received_message_.arg.executefile.filename);
                            Print("[ am ] execute application %s\n",
                                  received_message_.arg.executefile.filename);
                            SyscallSendMessage(&send_message_, fs_id_);
                            ChangeState(State::ExecuteFile);
                        }
                    } break;

                    case Message::kWrite: {
                        ChangeState(State::Write);
                        target_p_id_ =
                            app_manager_->GetPID(received_message_.src_task);
                        if (target_p_id_) {
                            if (received_message_.arg.write.fd == 1) {
                                send_message_ = received_message_;
                                SyscallSendMessage(&send_message_,
                                                   target_p_id_);
                            }
                        }
                        send_message_.type = Message::kReceived;
                        SyscallSendMessage(&send_message_,
                                           received_message_.src_task);
                        ChangeState(State::InitialState);
                    } break;

                    case Message::kOpen: {
                        // TODO: 1.flagの処理
                        auto [fs_id, err] = SyscallFindServer("servers/fs");
                        if (err) {
                            send_message_.type = Message::kError;
                            send_message_.arg.error.retry = false;
                            Print("[ am ] cannnot find file system server\n");
                            SyscallSendMessage(&send_message_,
                                               received_message_.src_task);
                        } else {
                            fs_id_ = fs_id;
                            target_id_ = received_message_.src_task;

                            send_message_.type = Message::kOpen;
                            strcpy(send_message_.arg.open.filename,
                                   received_message_.arg.open.filename);
                            SyscallSendMessage(&send_message_, fs_id_);
                            ChangeState(State::Open);
                        }
                    } break;

                    default:
                        break;
                }
            }
        } break;

        case State::ExecuteFile: {
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
                            SyscallSendMessage(&send_message_, target_p_id_);
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

        case State::Open: {
            ReceiveMessage();
            switch (received_message_.type) {
                case Message::kOpen: {
                    if (!received_message_.arg.open.exist) {
                        send_message_.type = Message::kOpen;
                        send_message_.arg.open.exist =
                            received_message_.arg.open.exist;
                    } else {
                        // 3.ファイルがあればファイルディスクリプタを作成する
                        // auto& app_info =
                        // app_manager_->GetAppInfo(received_message_.src_task);
                        // size_t fd = app_manager_->AllocateFD(app_info);
                        // 4.ファイルディスクリプタにファイル名をセット
                        // 5.返信
                        // send_message_.arg.open.fd = fd;
                    }
                    SyscallSendMessage(&send_message_, target_id_);
                    ChangeState(State::InitialState);
                } break;

                default:
                    Print("[ am ] unknown message type from fs server\n");
                    break;
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

extern "C" void main() {
    application_management_server = new ApplicationManagementServer;
    application_management_server->Initilize();

    while (true) {
        application_management_server->Processing();
    }
}
