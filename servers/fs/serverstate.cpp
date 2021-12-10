#include "serverstate.hpp"

#include <errno.h>
#include <fcntl.h>

#include <algorithm>
#include <cctype>
#include <utility>

#include "../../libs/kinos/common/print.hpp"
#include "../../libs/kinos/common/syscall.h"
#include "fs.hpp"

ServerState *ErrState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}

ServerState *InitState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->am_id_);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, server_->am_id_);
                    break;
                } else {
                    Print("[ fs ] error at am server\n");
                    return server_->GetServerState(State::StateErr);
                }
            } break;

            case Message::kExecuteFile: {
                return server_->GetServerState(State::StateExecFile);
            } break;

            case Message::kOpen:
            case Message::kOpenDir: {
                return server_->GetServerState(State::StateOpen);
            } break;

            case Message::kRead: {
                return server_->GetServerState(State::StateRead);
            } break;

            case Message::kWrite: {
                return server_->GetServerState(State::StateWrite);
            } break;

            default:
                Print("[ fs ] unknown message from am server");
                break;
        }
    }
}

ServerState *InitState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->am_id_);
    return this;
}

ServerState *ExecFileState::HandleMessage() {
    const char *path = server_->rm_.arg.executefile.filename;
    Print("[ fs ] find  %s\n", path);

    auto [file_entry, post_slash] = server_->FindFile(path);
    // the file doesn't exist
    if (!file_entry) {
        // find file in apps directory
        auto apps = server_->FindFile("apps");
        auto command = server_->FindFile(path, apps.first->FirstCluster());
        if (command.first) {
            server_->target_entry_ = command.first;
            server_->sm_.type = Message::kExecuteFile;
            server_->sm_.arg.executefile.exist = true;
            server_->sm_.arg.executefile.isdirectory = false;
            return this;
        } else {
            server_->sm_.type = Message::kError;
            server_->sm_.arg.error.retry = false;
            server_->sm_.arg.error.err = ENOENT;
            Print("[ fs ] cannnot find  %s\n", path);
            return server_->GetServerState(State::StateInit);
        }
    }

    // is a directory
    else if (file_entry->attr == Attribute::kDirectory) {
        server_->sm_.type = Message::kError;
        server_->sm_.arg.error.retry = false;
        server_->sm_.arg.error.err = EISDIR;
        return server_->GetServerState(State::StateInit);
    }

    // exists and is not a directory
    else {
        server_->target_entry_ = file_entry;
        server_->sm_.type = Message::kExecuteFile;
        server_->sm_.arg.executefile.exist = true;
        server_->sm_.arg.executefile.isdirectory = false;
        return this;
    }
}

ServerState *ExecFileState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->am_id_);
    return this;
}

ServerState *ExecFileState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, server_->am_id_);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, server_->am_id_);
                    continue;
                } else {
                    Print("[ fs ] error at am server\n");
                    return server_->GetServerState(State::StateErr);
                }
            } break;

            case Message::kExecuteFile: {
                return server_->GetServerState(State::StateExpandBuffer);
            }
            default:
                Print("[ fs ] unknown message from am server\n");
                return server_->GetServerState(State::StateErr);
        }
    }
}

ServerState *ExpandBufferState::HandleMessage() {
    server_->target_id_ = server_->rm_.arg.executefile.id;
    server_->sm_.type = Message::kExpandTaskBuffer;
    server_->sm_.arg.expand.id = server_->target_id_;
    server_->sm_.arg.expand.bytes = server_->target_entry_->file_size;
    return this;
}

ServerState *ExpandBufferState::SendMessage() {
    SyscallSendMessage(&server_->sm_, 1);
    return this;
}

ServerState *ExpandBufferState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->rm_, 1, 1);
        switch (server_->rm_.type) {
            case Message::kError: {
                if (server_->rm_.arg.error.retry) {
                    SyscallSendMessage(&server_->sm_, 1);
                    break;
                } else {
                    Print("[ fs ] error at kernel \n");
                    return server_->GetServerState(State::StateErr);
                }
            } break;

            case Message::kExpandTaskBuffer: {
                return server_->GetServerState(State::StateCopyToBuffer);
            }
            default:
                Print("[ fs ] unknown message from am server");
                return server_->GetServerState(State::StateErr);
        }
    }
}

ServerState *CopyToBufferState::HandleMessage() {
    auto cluster = server->target_entry_->FirstCluster();
    auto remain_bytes = server->target_entry_->file_size;
    int offset = 0;
    Print("[ fs ] copy %s to task buffer\n", server->target_entry_->name);

    while (cluster != 0 && cluster != 0x0ffffffflu) {
        char *p = reinterpret_cast<char *>(server->ReadCluster(cluster));
        if (remain_bytes >= server_->bytes_per_cluster_) {
            auto [res, err] = SyscallCopyToTaskBuffer(
                server->target_id_, p, offset, server_->bytes_per_cluster_);
            offset += server_->bytes_per_cluster_;
            remain_bytes -= server_->bytes_per_cluster_;
            cluster = server->NextCluster(cluster);

        } else {
            auto [res, err] = SyscallCopyToTaskBuffer(server->target_id_, p,
                                                      offset, remain_bytes);
            cluster = server->NextCluster(cluster);
        }
    }
    server->sm_.type = Message::kReady;
    return this;
}

ServerState *CopyToBufferState::SendMessage() {
    SyscallSendMessage(&server_->sm_, server_->am_id_);
    return server_->GetServerState(State::StateInit);
}

ServerState *OpenState::HandleMessage() {
    if (server_->rm_.type == Message::kOpen) SetTarget(Target::File);
    if (server_->rm_.type == Message::kOpenDir) SetTarget(Target::Dir);
    switch (target_) {
        case Target::File: {
            const char *path = server_->rm_.arg.open.filename;
            Print("[ fs ] find file %s\n", path);
            auto [file_entry, post_slash] = server_->FindFile(path);
            // the file doesn't exist
            if (!file_entry) {
                if ((server_->rm_.arg.open.flags & O_CREAT) == 0) {
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = ENOENT;
                    Print("[ fs ] cannnot find  %s\n", path);
                } else {
                    auto [new_file, err] = server_->CreateFile(path);
                    if (err) {
                        server_->sm_.type = Message::kError;
                        server_->sm_.arg.error.retry = false;
                        server_->sm_.arg.error.err = err;
                        Print("[ fs ] cannnot create file \n");
                    } else {
                        // success to create file
                        Print("[ fs ] create file %s\n", path);
                        server_->sm_.type = Message::kOpen;
                        strcpy(server_->sm_.arg.open.filename,
                               server_->rm_.arg.open.filename);
                        server_->sm_.arg.open.exist = true;
                        server_->sm_.arg.open.isdirectory = false;
                    }
                }
            }
            // is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                server_->sm_.type = Message::kError;
                server_->sm_.arg.error.retry = false;
                server_->sm_.arg.error.err = EISDIR;
            }
            // exists and is not a directory
            else {
                server_->sm_.type = Message::kOpen;
                strcpy(server_->sm_.arg.open.filename,
                       server_->rm_.arg.open.filename);
                server_->sm_.arg.open.exist = true;
                server_->sm_.arg.open.isdirectory = false;
            }
            return server_->GetServerState(State::StateInit);
        } break;

        case Target::Dir: {
            const char *path = server_->rm_.arg.opendir.dirname;
            Print("[ fs ] find directory %s\n", path);
            // is root directory
            if (strcmp(path, "/") == 0) {
                server_->sm_.type = Message::kOpenDir;
                strcpy(server_->sm_.arg.opendir.dirname,
                       server_->rm_.arg.opendir.dirname);
                server_->sm_.arg.opendir.exist = true;
                server_->sm_.arg.opendir.isdirectory = true;
            } else {
                auto [file_entry, post_slash] = server_->FindFile(path);
                // the directory doesn't exist
                if (!file_entry) {
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = ENOENT;
                    Print("[ fs ] cannnot find  %s\n", path);
                }
                //  is directory
                else if (file_entry->attr == Attribute::kDirectory) {
                    server_->sm_.type = Message::kOpenDir;
                    strcpy(server_->sm_.arg.opendir.dirname,
                           server_->rm_.arg.opendir.dirname);
                    server_->sm_.arg.opendir.exist = true;
                    server_->sm_.arg.opendir.isdirectory = true;
                }
                // not directory
                else {
                    server_->sm_.type = Message::kError;
                    server_->sm_.arg.error.retry = false;
                    server_->sm_.arg.error.err = ENOTDIR;
                    Print("[ fs ] not directory  %s\n", path);
                }
            }
            return server_->GetServerState(State::StateInit);
        } break;

        default:
            break;
    }
}

ServerState *ReadState::HandleMessage() {
    const char *path = server_->rm_.arg.read.filename;
    // root directory
    if (strcmp(path, "/") == 0) {
        auto cluster = server_->bpb_.root_cluster;
        size_t read_cluster = server_->rm_.arg.read.cluster;
        for (int i = 0; i < read_cluster; ++i) {
            cluster = server_->NextCluster(cluster);
        }
        size_t entry_index = server_->rm_.arg.read.offset;
        auto num_entries_per_cluster = (server_->bpb_.bytes_per_sector *
                                        server_->bpb_.sectors_per_cluster) /
                                       sizeof(DirectoryEntry);
        if (num_entries_per_cluster <= entry_index) {
            read_cluster = entry_index / num_entries_per_cluster;
            entry_index = entry_index % num_entries_per_cluster;
        }

        if (cluster != 0 && cluster != 0x0ffffffflu) {
            DirectoryEntry *dir_entry = reinterpret_cast<DirectoryEntry *>(
                server_->ReadCluster(cluster));

            char name[9];
            char ext[4];
            while (1) {
                server_->ReadName(dir_entry[entry_index], name, ext);
                if (name[0] == 0x00) {
                    break;
                } else if (static_cast<uint8_t>(name[0]) == 0xe5) {
                    ++entry_index;
                    continue;
                } else if (dir_entry[entry_index].attr ==
                           Attribute::kLongName) {
                    ++entry_index;
                    continue;
                } else {
                    ++entry_index;
                    server_->sm_.type = Message::kRead;
                    memcpy(&server_->sm_.arg.read.data[0], &name[0], 9);
                    server_->sm_.arg.read.len =
                        (entry_index + num_entries_per_cluster * read_cluster) -
                        server_->rm_.arg.read.offset;
                    server_->sm_.arg.read.cluster = read_cluster;
                    SyscallSendMessage(&server_->sm_, server_->am_id_);
                    break;
                }
            }
        }
        goto finish;

    } else {
        auto [file_entry, post_slash] = server_->FindFile(path);

        if (file_entry->attr == Attribute::kDirectory) {
            server_->target_entry_ = file_entry;
            auto cluster = server_->target_entry_->FirstCluster();
            size_t read_cluster = server_->rm_.arg.read.cluster;
            for (int i = 0; i < read_cluster; ++i) {
                cluster = server_->NextCluster(cluster);
            }

            size_t entry_index = server_->rm_.arg.read.offset;
            auto num_entries_per_cluster = (server_->bpb_.bytes_per_sector *
                                            server_->bpb_.sectors_per_cluster) /
                                           sizeof(DirectoryEntry);
            if (num_entries_per_cluster <= entry_index) {
                read_cluster = entry_index / num_entries_per_cluster;
                entry_index = entry_index % num_entries_per_cluster;
            }

            if (cluster != 0 && cluster != 0x0ffffffflu) {
                DirectoryEntry *dir_entry = reinterpret_cast<DirectoryEntry *>(
                    server_->ReadCluster(cluster));

                char name[9];
                char ext[4];
                while (1) {
                    server_->ReadName(dir_entry[entry_index], name, ext);
                    if (name[0] == 0x00) {
                        break;
                    } else if (static_cast<uint8_t>(name[0]) == 0xe5) {
                        ++entry_index;
                        continue;
                    } else if (dir_entry[entry_index].attr ==
                               Attribute::kLongName) {
                        ++entry_index;
                        continue;
                    } else {
                        ++entry_index;
                        server_->sm_.type = Message::kRead;
                        memcpy(&server_->sm_.arg.read.data[0], &name[0], 9);
                        server_->sm_.arg.read.len =
                            (entry_index +
                             num_entries_per_cluster * read_cluster) -
                            server_->rm_.arg.read.offset;
                        server_->sm_.arg.read.cluster = read_cluster;
                        SyscallSendMessage(&server_->sm_, server_->am_id_);
                        break;
                    }
                }
            }
            goto finish;

        } else {
            server_->target_entry_ = file_entry;

            size_t count = server_->rm_.arg.read.count;
            size_t sent_bytes = 0;
            size_t read_offset = server_->rm_.arg.read.offset;

            size_t total = 0;
            auto cluster = server_->target_entry_->FirstCluster();
            auto remain_bytes = server_->target_entry_->file_size;
            int msg_offset = 0;

            while (cluster != 0 && cluster != 0x0ffffffflu &&
                   sent_bytes < count) {
                char *p =
                    reinterpret_cast<char *>(server_->ReadCluster(cluster));
                int i = 0;
                for (; i < server_->bytes_per_cluster_ && i < remain_bytes &&
                       sent_bytes < count;
                     ++i) {
                    if (total >= read_offset) {
                        memcpy(&server_->sm_.arg.read.data[msg_offset], p, 1);
                        ++p;
                        ++msg_offset;
                        ++sent_bytes;

                        if (msg_offset == sizeof(server_->sm_.arg.read.data)) {
                            server_->sm_.type = Message::kRead;
                            server_->sm_.arg.read.len = msg_offset;
                            SyscallSendMessage(&server_->sm_, server_->am_id_);
                            msg_offset = 0;
                        }
                    }
                    ++total;
                }
                remain_bytes -= i;
                cluster = server_->NextCluster(cluster);
            }

            if (msg_offset) {
                server_->sm_.type = Message::kRead;
                server_->sm_.arg.read.len = msg_offset;
                SyscallSendMessage(&server_->sm_, server_->am_id_);
                msg_offset = 0;
            }
            goto finish;
        }
    }

finish:
    // finish reading
    server_->sm_.type = Message::kRead;
    server_->sm_.arg.read.len = 0;
    return server_->GetServerState(State::StateInit);
}

ServerState *WriteState::HandleMessage() {
    const char *path = server_->rm_.arg.write.filename;
    size_t num_cluster =
        (server_->rm_.arg.write.count + server_->bytes_per_cluster_ - 1) /
        server_->bytes_per_cluster_;

    auto [file_entry, post_slash] = server_->FindFile(path);
    server_->target_entry_ = file_entry;
    unsigned long cluster;
    if (server_->target_entry_->FirstCluster() != 0) {
        cluster = server_->target_entry_->FirstCluster();
    } else {
        cluster = server_->AllocateClusterChain(num_cluster);
        server_->target_entry_->first_cluster_low = cluster & 0xffff;
        server_->target_entry_->first_cluster_high = (cluster >> 16) & 0xffff;
        server_->UpdateCluster();
    }
    size_t len = server_->rm_.arg.write.len;

    // finish writing
    if (len == 0) {
        server_->target_entry_->file_size = server_->rm_.arg.write.offset;
        server_->UpdateCluster();
        return server_->GetServerState(State::StateWrite);
    }

    size_t data_off = 0;
    size_t write_off =
        server_->rm_.arg.write.offset % server_->bytes_per_cluster_;
    size_t cluster_off =
        server_->rm_.arg.write.offset / server_->bytes_per_cluster_;

    for (int i = 0; i < cluster_off; ++i) {
        auto next_cluster = server_->NextCluster(cluster);
        if (next_cluster == 0x0ffffffflu) {
            cluster = server_->ExtendCluster(cluster, 1);
        } else {
            cluster = next_cluster;
        }
    }

    while (data_off < len) {
        if (write_off == server_->bytes_per_cluster_) {
            auto next_cluster = server_->NextCluster(cluster);
            if (next_cluster == 0x0ffffffflu) {
                cluster = server_->ExtendCluster(cluster, 1);
            } else {
                cluster = next_cluster;
            }
            write_off = 0;
        }

        uint8_t *sec =
            reinterpret_cast<uint8_t *>(server_->ReadCluster(cluster));
        size_t n = std::min(len, server_->bytes_per_cluster_ - write_off);
        memcpy(&sec[write_off], &server_->rm_.arg.write.data[data_off], n);
        data_off += n;

        write_off += n;
        server_->UpdateCluster(cluster);
    }

    return server_->GetServerState(State::StateWrite);
}

ServerState *WriteState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}
