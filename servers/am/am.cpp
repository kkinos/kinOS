#include "am.hpp"

#include <cstdio>

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

void ProcessAccordingToMessage() {
    // message from kernel
    if (received_message[0].src_task == 1) {
        switch (received_message[0].type) {
            case Message::kExcute: {
                if (!received_message[0].arg.execute.success) {
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    uint64_t pid =
                        app_manager->GetPID(received_message[0].arg.execute.id);
                    if (pid != 0) {
                        SyscallSendMessage(sent_message, pid);
                    }
                }
                goto end;
            }

            case Message::kExit: {
                uint64_t pid =
                    app_manager->GetPID(received_message[0].arg.exit.id);
                if (pid != 0) {
                    sent_message[0].type = Message::kExit;
                    sent_message[0].arg.exit.result =
                        received_message[0].arg.exit.result;
                    SyscallSendMessage(sent_message, pid);
                    SyscallWriteKernelLog("[ am ] close application\n");
                }
                goto end;
            }

            default:
                SyscallWriteKernelLog(
                    "[ am ] Unknown message type from kernel \n");
                goto end;
        }

        // message from others
    } else {
        switch (received_message[0].type) {
            case Message::kExecuteFile: {
                uint64_t p_id = received_message[0].src_task;
                char arg[32];
                strcpy(arg, received_message[0].arg.executefile.arg);

                auto [fs_id, err] = SyscallFindServer("servers/fs");
                if (err) {
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    SyscallWriteKernelLog(
                        "[ am ] cannnot find file system server\n");
                    SyscallSendMessage(sent_message, p_id);
                }

                sent_message[0].type = Message::kExecuteFile;
                strcpy(sent_message[0].arg.executefile.filename,
                       received_message[0].arg.executefile.filename);

                SyscallSendMessage(sent_message, fs_id);
                CreateNewTask(p_id, fs_id, arg);
                goto end;
            }

            case Message::kWrite: {
                uint64_t pid =
                    app_manager->GetPID(received_message[0].src_task);
                if (pid != 0) {
                    if (received_message[0].arg.write.fd == 1) {
                        sent_message[0] = received_message[0];
                        SyscallSendMessage(sent_message, pid);
                    }
                }
                goto end;
            }

            default:
                SyscallWriteKernelLog("[ am ] Unknown message type\n");
                goto end;
        }
    }

end:
    return;
}

void CreateNewTask(uint64_t p_id, uint64_t fs_id, char* arg) {
    while (true) {
        SyscallClosedReceiveMessage(received_message, 1, fs_id);
        switch (received_message[0].type) {
            case Message::kError:
                if (received_message[0].arg.error.retry) {
                    SyscallSendMessage(sent_message, fs_id);
                    break;
                } else {
                    SyscallWriteKernelLog("[ am ] error at fs server\n");
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    SyscallSendMessage(sent_message, p_id);
                    goto end;
                }
            case Message::kExecuteFile: {
                if (!received_message[0].arg.executefile.exist ||
                    received_message[0].arg.executefile.directory) {
                    sent_message[0].type = Message::kExecuteFile;
                    sent_message[0].arg.executefile.exist =
                        received_message[0].arg.executefile.exist;
                    sent_message[0].arg.executefile.directory =
                        received_message[0].arg.executefile.directory;
                    SyscallSendMessage(sent_message, p_id);
                    goto end;
                }
                // the file exists, and not a directory
                else {
                    auto [id, err] = SyscallCreateNewTask();
                    if (err) {
                        SyscallWriteKernelLog("[ am ] syscall Error\n");
                        sent_message[0].type = Message::kError;
                        sent_message[0].arg.error.retry = false;
                        SyscallSendMessage(sent_message, p_id);
                        SyscallSendMessage(sent_message, fs_id);
                        goto end;
                    }
                    SyscallWriteKernelLog("[ am ] create task\n");
                    sent_message[0].type = Message::kExecuteFile;
                    sent_message[0].arg.executefile.id = id;
                    SyscallSendMessage(sent_message, fs_id);
                    ExecuteAppTask(p_id, id, arg, fs_id);
                    goto end;
                }
            }

            default:
                SyscallWriteKernelLog(
                    "[ am ] Unknown message type from fs server\n");
                goto end;
        }
    }
end:
    return;
}

void ExecuteAppTask(uint64_t p_id, uint64_t id, char* arg, uint64_t fs_id) {
    while (true) {
        SyscallClosedReceiveMessage(received_message, 1, fs_id);
        switch (received_message[0].type) {
            case Message::kError:
                if (received_message[0].arg.error.retry) {
                    SyscallSendMessage(sent_message, fs_id);
                    break;
                } else {
                    SyscallWriteKernelLog("[ am ] error at other server\n");
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    SyscallSendMessage(sent_message, p_id);
                    goto end;
                }

            case Message::kReady: {
                auto [res, err] = SyscallSetArgument(id, arg);
                if (err) {
                    SyscallWriteKernelLog("[ am ] Syscall error\n");
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    SyscallSendMessage(sent_message, p_id);
                    goto end;
                }
                app_manager->NewApp(id, p_id);

                sent_message[0].type = Message::kExcute;
                sent_message[0].arg.execute.id = id;
                sent_message[0].arg.execute.p_id = p_id;
                sent_message[0].arg.execute.success = true;
                SyscallSendMessage(sent_message, 1);
                goto end;
            }

            default:
                SyscallWriteKernelLog(
                    "[ am ] Unknown message type from fs server\n");
                goto end;
        }
    }
end:
    return;
}

extern "C" void main() {
    app_manager = new AppManager;

    SyscallWriteKernelLog("[ am ] ready\n");

    while (true) {
        SyscallOpenReceiveMessage(received_message, 1);
        ProcessAccordingToMessage();
    }
}
