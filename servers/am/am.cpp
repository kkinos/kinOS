#include "am.hpp"

#include <cstdio>

void ProcessAccordingToMessage(Message* msg) {
    // message from kernel
    if (msg->src_task == 1) {
        switch (msg->type) {
            case Message::kExcute: {
                if (!msg->arg.execute.success) {
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    if (auto it = am_table->find(msg->arg.execute.id);
                        it != am_table->end()) {
                        SyscallSendMessage(sent_message, it->second);
                    }
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
        switch (msg->type) {
            case Message::kExecuteFile: {
                uint64_t p_id = msg->src_task;
                char arg[32];
                strcpy(arg, msg->arg.executefile.arg);

                auto [fs_id, err] = SyscallFindServer("servers/fs");
                if (err) {
                    sent_message[0].type = Message::kError;
                    sent_message[0].arg.error.retry = false;
                    SyscallWriteKernelLog(
                        "[ am ] cannnot find file system server\n");
                    SyscallSendMessage(sent_message, p_id);
                }

                sent_message[0] = *msg;
                SyscallSendMessage(sent_message, fs_id);
                CreateNewTask(p_id, fs_id, arg);
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
                    sent_message[0] = received_message[0];
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
                    sent_message[0] = received_message[0];
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
                am_table->insert(std::make_pair(id, p_id));
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
    am_table = new std::map<uint64_t, uint64_t>;
    SyscallWriteKernelLog("[ am ] ready\n");

    while (true) {
        SyscallOpenReceiveMessage(received_message, 1);
        ProcessAccordingToMessage(received_message);
    }
}
