#include "am.hpp"

#include <cstdio>

extern "C" void main() {
    am_table = new std::map<uint64_t, uint64_t>;

    SyscallWriteKernelLog("[ am ] ready\n");

    while (true) {
        SyscallOpenReceiveMessage(rmsg, 1);

        /* カーネルからのメッセージ */
        if (rmsg[0].src_task == 1) {
            if (rmsg[0].type == Message::kExcute) {
                if (!rmsg[0].arg.execute.success) {
                    smsg[0].type = Message::Error;
                    smsg[0].arg.error.retry = false;
                    if (auto it = am_table->find(rmsg[0].arg.execute.id);
                        it != am_table->end()) {
                        SyscallSendMessage(smsg, it->second);
                    }
                }
            }
        }

        /* それ以外 */
        else if (rmsg[0].type == Message::aExecuteFile) {
            uint64_t p_id = rmsg[0].src_task;
            char arg[32];
            strcpy(arg, rmsg[0].arg.executefile.arg);

            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                smsg[0].type = Message::Error;
                smsg[0].arg.error.retry = false;
                SyscallWriteKernelLog(
                    "[ am ] cannnot find file system server\n");
                SyscallSendMessage(smsg, p_id);
            }

            smsg[0] = rmsg[0];
            SyscallSendMessage(smsg, fs_id);

            while (true) {
                SyscallClosedReceiveMessage(rmsg, 1, fs_id);

                if (rmsg[0].type == Message::Error) {
                    if (rmsg[0].arg.error.retry) {
                        SyscallSendMessage(smsg, fs_id);
                    } else {
                        SyscallWriteKernelLog("[ am ] error at other server\n");
                        smsg[0].type = Message::Error;
                        smsg[0].arg.error.retry = false;
                        SyscallSendMessage(smsg, p_id);
                        break;
                    }
                } else if (rmsg[0].type == Message::aExecuteFile) {
                    /* 指定されたファイルが存在しないかディレクトリであるとき */
                    if (!rmsg[0].arg.executefile.exist ||
                        rmsg[0].arg.executefile.directory) {
                        smsg[0] = rmsg[0];
                        SyscallSendMessage(smsg, p_id);
                        break;
                    }
                    /* 指定されたファイルが実行可能であるとき */
                    else {
                        SyscallWriteKernelLog("[ am ] create task\n");
                        auto [id, err] =
                            SyscallNewTask();  // 新しいタスクを生成
                        if (err) {
                            SyscallWriteKernelLog("[ am ] syscall Error\n");
                            smsg[0].type = Message::Error;
                            smsg[0].arg.error.retry = false;
                            SyscallSendMessage(smsg, p_id);
                            SyscallSendMessage(smsg, fs_id);
                            break;
                        }
                        /* 新しいタスクのidをfsサーバに伝える */
                        smsg[0] = rmsg[0];
                        smsg[0].arg.executefile.id = id;
                        SyscallSendMessage(smsg, fs_id);

                        while (true) {
                            SyscallClosedReceiveMessage(rmsg, 1, fs_id);

                            if (rmsg[0].type == Message::Error) {
                                if (rmsg[0].arg.error.retry) {
                                    SyscallSendMessage(smsg, fs_id);
                                } else {
                                    SyscallWriteKernelLog(
                                        "[ am ] Error at other server\n");
                                    smsg[0].type = Message::Error;
                                    smsg[0].arg.error.retry = false;
                                    SyscallSendMessage(smsg, p_id);
                                    break;
                                }
                            }
                            if (rmsg[0].type == Message::Ready) {
                                auto [res, err] = SyscallSetArgument(id, arg);
                                if (err) {
                                    SyscallWriteKernelLog(
                                        "[ am ] Syscall Error\n");
                                    smsg[0].type = Message::Error;
                                    smsg[0].arg.error.retry = false;
                                    SyscallSendMessage(smsg, p_id);
                                    break;
                                }
                                am_table->insert(std::make_pair(id, p_id));
                                smsg[0].type = Message::kExcute;
                                smsg[0].arg.execute.id = id;
                                smsg[0].arg.execute.p_id = p_id;
                                smsg[0].arg.execute.success = true;
                                SyscallSendMessage(smsg, 1);
                                break;
                            }
                        }

                        break;
                    }
                }
            }
        }
    }
}