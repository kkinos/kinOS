#include "am.hpp"

#include <cstdio>

extern "C" void main() {
    SyscallWriteKernelLog("am: Start\n");

    while (true) {
        SyscallOpenReceiveMessage(rmsg, 1);

        if (rmsg[0].type == Message::aExecuteFile) {
            uint64_t task_id = rmsg[0].src_task;
            auto [fs_id, err] = SyscallFindServer("servers/fs");
            if (err) {
                smsg[0].type = Message::Error;
                smsg[0].arg.error.retry = false;
                SyscallWriteKernelLog("am: cannnot find file system server\n");
                SyscallSendMessage(smsg, task_id);
            }

            smsg[0] = rmsg[0];
            SyscallSendMessage(smsg, fs_id);

            while (true) {
                SyscallClosedReceiveMessage(rmsg, 1, fs_id);

                if (rmsg[0].type == Message::Error) {
                    if (rmsg[0].arg.error.retry) {
                        SyscallSendMessage(smsg, fs_id);
                    } else {
                        SyscallWriteKernelLog("am: Error at other server\n");
                        smsg[0].type = Message::Error;
                        smsg[0].arg.error.retry = false;
                        SyscallSendMessage(smsg, task_id);
                        break;
                    }
                } else if (rmsg[0].type == Message::aExecuteFile) {
                    // 指定されたファイルが存在しないかディレクトリであるとき
                    if (!rmsg[0].arg.executefile.exist ||
                        rmsg[0].arg.executefile.directory) {
                        smsg[0] = rmsg[0];
                        SyscallSendMessage(smsg, task_id);
                        break;
                    }
                    // 指定されたファイルが実行可能であるとき
                    else {
                        SyscallWriteKernelLog("am: CreateTask\n");
                        auto [id, err] = SyscallNewTask();
                        if (err) {
                            SyscallWriteKernelLog("am: Syscall Error\n");
                            smsg[0].type = Message::Error;
                            smsg[0].arg.error.retry = false;
                            SyscallSendMessage(smsg, task_id);
                            SyscallSendMessage(smsg, fs_id);
                            break;
                        }
                        smsg[0] = rmsg[0];
                        smsg[0].arg.executefile.id = id;
                        SyscallSendMessage(smsg, fs_id);
                        break;
                    }
                }
            }
        }
    }
}