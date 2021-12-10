#include "filedescriptor.hpp"

#include <cstring>

#include "../../libs/kinos/common/print.hpp"
#include "../../libs/kinos/common/syscall.h"

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
