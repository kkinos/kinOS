#include "fs.hpp"

#include <errno.h>
#include <fcntl.h>

#include <algorithm>
#include <cctype>
#include <utility>

#include "../../libs/common/message.hpp"
#include "../../libs/kinos/common/print.hpp"
#include "../../libs/kinos/common/syscall.h"

extern "C" void main() {
    server = new FileSystemServer;
    server->Initialize();
    while (true) {
        server->ReceiveMessage();
        server->HandleMessage();
        server->SendMessage();
    }
}

ErrState::ErrState(FileSystemServer *server) : server_{server} {}

ServerState *ErrState::SendMessage() {
    return server_->GetServerState(State::StateInit);
}

InitState::InitState(FileSystemServer *server) : server_{server} {}

ServerState *InitState::ReceiveMessage() {
    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        Print("[ fs ] cannot find  application management server\n");
        return server_->GetServerState(State::StateErr);
    }
    server_->am_id_ = am_id;

    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1, am_id);
        switch (server_->received_message_.type) {
            case Message::kError: {
                if (server_->received_message_.arg.error.retry) {
                    SyscallSendMessage(&server_->send_message_, am_id);
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

            default:
                Print("[ fs ] unknown message from am server");
                break;
        }
    }
}

ServerState *InitState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->am_id_);
    return this;
}

ExecFileState::ExecFileState(FileSystemServer *server) : server_{server} {}

ServerState *ExecFileState::ReceiveMessage() {
    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        Print("[ fs ] cannot find  application management server\n");
        return server_->GetServerState(State::StateErr);
    }
    server_->am_id_ = am_id;

    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1, am_id);
        switch (server_->received_message_.type) {
            case Message::kError: {
                if (server_->received_message_.arg.error.retry) {
                    SyscallSendMessage(&server_->send_message_, am_id);
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

ServerState *ExecFileState::HandleMessage() {
    const char *path = server_->received_message_.arg.executefile.filename;
    Print("[ fs ] find  %s\n", path);

    auto [file_entry, post_slash] = server_->FindFile(path);
    // the file doesn't exist
    if (!file_entry) {
        server_->send_message_.type = Message::kExecuteFile;
        server_->send_message_.arg.executefile.exist = false;
        server_->send_message_.arg.executefile.isdirectory = false;
        Print("[ fs ] cannnot find  %s\n", path);
        return server_->GetServerState(State::StateInit);
    }

    // is a directory
    else if (file_entry->attr == Attribute::kDirectory) {
        server_->send_message_.type = Message::kExecuteFile;
        server_->send_message_.arg.executefile.exist = true;
        server_->send_message_.arg.executefile.isdirectory = true;
        return server_->GetServerState(State::StateInit);
    }

    // exists and is not a directory
    else {
        server_->target_file_entry_ = file_entry;
        server_->send_message_.type = Message::kExecuteFile;
        server_->send_message_.arg.executefile.exist = true;
        server_->send_message_.arg.executefile.isdirectory = false;
        return this;
    }
}

ServerState *ExecFileState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->am_id_);
    return this;
}

ExpandBufferState::ExpandBufferState(FileSystemServer *server)
    : server_{server} {}

ServerState *ExpandBufferState::ReceiveMessage() {
    while (1) {
        SyscallClosedReceiveMessage(&server_->received_message_, 1, 1);
        switch (server_->received_message_.type) {
            case Message::kError: {
                if (server_->received_message_.arg.error.retry) {
                    SyscallSendMessage(&server_->send_message_, 1);
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
ServerState *ExpandBufferState::HandleMessage() {
    server_->target_task_id_ = server_->received_message_.arg.executefile.id;
    server_->send_message_.type = Message::kExpandTaskBuffer;
    server_->send_message_.arg.expand.id = server_->target_task_id_;
    server_->send_message_.arg.expand.bytes =
        server_->target_file_entry_->file_size;
    return this;
}

ServerState *ExpandBufferState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, 1);
    return this;
}

CopyToBufferState::CopyToBufferState(FileSystemServer *server)
    : server_{server} {}

ServerState *CopyToBufferState::HandleMessage() {
    auto cluster = server->target_file_entry_->FirstCluster();
    auto remain_bytes = server->target_file_entry_->file_size;
    int offset = 0;
    Print("[ fs ] copy %s to task buffer\n", server->target_file_entry_->name);

    while (cluster != 0 && cluster != 0x0ffffffflu) {
        char *p = reinterpret_cast<char *>(server->ReadCluster(cluster));
        if (remain_bytes >= server_->bytes_per_cluster_) {
            auto [res, err] =
                SyscallCopyToTaskBuffer(server->target_task_id_, p, offset,
                                        server_->bytes_per_cluster_);
            offset += server_->bytes_per_cluster_;
            remain_bytes -= server_->bytes_per_cluster_;
            cluster = server->NextCluster(cluster);

        } else {
            auto [res, err] = SyscallCopyToTaskBuffer(server->target_task_id_,
                                                      p, offset, remain_bytes);
            cluster = server->NextCluster(cluster);
        }
    }
    server->send_message_.type = Message::kReady;
    return this;
}

ServerState *CopyToBufferState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->am_id_);
    return server_->GetServerState(State::StateInit);
}

OpenState::OpenState(FileSystemServer *server) : server_{server} {}

ServerState *OpenState::HandleMessage() {
    if (server_->received_message_.type == Message::kOpen)
        SetTarget(Target::File);
    if (server_->received_message_.type == Message::kOpenDir)
        SetTarget(Target::Dir);
    switch (target_) {
        case Target::File: {
            const char *path = server_->received_message_.arg.open.filename;
            Print("[ fs ] find file %s\n", path);
            auto [file_entry, post_slash] = server_->FindFile(path);
            // the file doesn't exist
            if (!file_entry) {
                if ((server_->received_message_.arg.open.flags & O_CREAT) ==
                    0) {
                    server_->send_message_.type = Message::kOpen;
                    server_->send_message_.arg.open.exist = false;
                    server_->send_message_.arg.open.isdirectory = false;
                    Print("[ fs ] cannnot find  %s\n", path);
                } else {
                    auto [new_file, err] = server_->CreateFile(path);
                    if (err) {
                        server_->send_message_.type = Message::kError;
                        server_->send_message_.arg.error.retry = false;
                        server_->send_message_.arg.error.err = err;
                        Print("[ fs ] cannnot create file \n");
                    } else {
                        // success to create file
                        Print("[ fs ] create file %s\n", path);
                        server_->send_message_.type = Message::kOpen;
                        strcpy(server_->send_message_.arg.open.filename,
                               server_->received_message_.arg.open.filename);
                        server_->send_message_.arg.open.exist = true;
                        server_->send_message_.arg.open.isdirectory = false;
                    }
                }
            }
            // is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                server_->send_message_.type = Message::kOpen;
                strcpy(server_->send_message_.arg.open.filename,
                       server_->received_message_.arg.open.filename);
                server_->send_message_.arg.open.exist = true;
                server_->send_message_.arg.open.isdirectory = true;
            }
            // exists and is not a directory
            else {
                server_->send_message_.type = Message::kOpen;
                strcpy(server_->send_message_.arg.open.filename,
                       server_->received_message_.arg.open.filename);
                server_->send_message_.arg.open.exist = true;
                server_->send_message_.arg.open.isdirectory = false;
            }
            return server_->GetServerState(State::StateInit);
        } break;

        case Target::Dir: {
            const char *path = server_->received_message_.arg.opendir.dirname;
            Print("[ fs ] find directory %s\n", path);
            // is root directory
            if (strcmp(path, ".") == 0) {
                server_->send_message_.type = Message::kOpenDir;
                strcpy(server_->send_message_.arg.opendir.dirname,
                       server_->received_message_.arg.opendir.dirname);
                server_->send_message_.arg.opendir.exist = true;
                server_->send_message_.arg.opendir.isdirectory = true;
            } else {
                auto [file_entry, post_slash] = server_->FindFile(path);
                // the directory doesn't exist
                if (!file_entry) {
                    server_->send_message_.type = Message::kOpenDir;
                    server_->send_message_.arg.opendir.exist = false;
                    server_->send_message_.arg.opendir.isdirectory = false;
                    Print("[ fs ] cannnot find  %s\n", path);
                }
                //  is directory
                else if (file_entry->attr == Attribute::kDirectory) {
                    server_->send_message_.type = Message::kOpenDir;
                    strcpy(server_->send_message_.arg.opendir.dirname,
                           server_->received_message_.arg.opendir.dirname);
                    server_->send_message_.arg.opendir.exist = true;
                    server_->send_message_.arg.opendir.isdirectory = true;
                }
                // not directory
                else {
                    server_->send_message_.type = Message::kOpenDir;
                    strcpy(server_->send_message_.arg.opendir.dirname,
                           server_->received_message_.arg.opendir.dirname);
                    server_->send_message_.arg.opendir.exist = true;
                    server_->send_message_.arg.opendir.isdirectory = false;
                }
            }
            return server_->GetServerState(State::StateInit);
        } break;

        default:
            break;
    }
}

ReadState::ReadState(FileSystemServer *server) : server_{server} {}

ServerState *ReadState::HandleMessage() {
    const char *path = server_->received_message_.arg.read.filename;
    // root directory
    if (strcmp(path, ".") == 0) {
        auto cluster = server_->boot_volume_image_.root_cluster;
        size_t read_cluster = server_->received_message_.arg.read.cluster;
        for (int i = 0; i < read_cluster; ++i) {
            cluster = server_->NextCluster(cluster);
        }
        size_t entry_index = server_->received_message_.arg.read.offset;
        auto num_entries_per_cluster =
            (server_->boot_volume_image_.bytes_per_sector *
             server_->boot_volume_image_.sectors_per_cluster) /
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
                    server_->send_message_.type = Message::kRead;
                    memcpy(&server_->send_message_.arg.read.data[0], &name[0],
                           9);
                    server_->send_message_.arg.read.len =
                        (entry_index + num_entries_per_cluster * read_cluster) -
                        server_->received_message_.arg.read.offset;
                    server_->send_message_.arg.read.cluster = read_cluster;
                    SyscallSendMessage(&server_->send_message_,
                                       server_->am_id_);
                    break;
                }
            }
        }
        server_->send_message_.type = Message::kRead;
        server_->send_message_.arg.read.len = 0;
        return server_->GetServerState(State::StateInit);
    }

    auto [file_entry, post_slash] = server_->FindFile(path);

    if (file_entry->attr == Attribute::kDirectory) {
        server_->target_file_entry_ = file_entry;
        auto cluster = server_->target_file_entry_->FirstCluster();
        size_t read_cluster = server_->received_message_.arg.read.cluster;
        for (int i = 0; i < read_cluster; ++i) {
            cluster = server_->NextCluster(cluster);
        }

        size_t entry_index = server_->received_message_.arg.read.offset;
        auto num_entries_per_cluster =
            (server_->boot_volume_image_.bytes_per_sector *
             server_->boot_volume_image_.sectors_per_cluster) /
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
                    server_->send_message_.type = Message::kRead;
                    memcpy(&server_->send_message_.arg.read.data[0], &name[0],
                           9);
                    server_->send_message_.arg.read.len =
                        (entry_index + num_entries_per_cluster * read_cluster) -
                        server_->received_message_.arg.read.offset;
                    server_->send_message_.arg.read.cluster = read_cluster;
                    SyscallSendMessage(&server_->send_message_,
                                       server_->am_id_);
                    break;
                }
            }
        }
        server_->send_message_.type = Message::kRead;
        server_->send_message_.arg.read.len = 0;
        return server_->GetServerState(State::StateInit);

    } else {
        server_->target_file_entry_ = file_entry;

        size_t count = server_->received_message_.arg.read.count;
        size_t sent_bytes = 0;
        size_t read_offset = server_->received_message_.arg.read.offset;

        size_t total = 0;
        auto cluster = server_->target_file_entry_->FirstCluster();
        auto remain_bytes = server_->target_file_entry_->file_size;
        int msg_offset = 0;

        while (cluster != 0 && cluster != 0x0ffffffflu && sent_bytes < count) {
            char *p = reinterpret_cast<char *>(server_->ReadCluster(cluster));
            int i = 0;
            for (; i < server_->bytes_per_cluster_ && i < remain_bytes &&
                   sent_bytes < count;
                 ++i) {
                if (total >= read_offset) {
                    memcpy(&server_->send_message_.arg.read.data[msg_offset], p,
                           1);
                    ++p;
                    ++msg_offset;
                    ++sent_bytes;

                    if (msg_offset ==
                        sizeof(server_->send_message_.arg.read.data)) {
                        server_->send_message_.type = Message::kRead;
                        server_->send_message_.arg.read.len = msg_offset;
                        SyscallSendMessage(&server_->send_message_,
                                           server_->am_id_);
                        msg_offset = 0;
                    }
                }
                ++total;
            }
            remain_bytes -= i;
            cluster = server_->NextCluster(cluster);
        }

        if (msg_offset) {
            server_->send_message_.type = Message::kRead;
            server_->send_message_.arg.read.len = msg_offset;
            SyscallSendMessage(&server_->send_message_, server_->am_id_);
            msg_offset = 0;
        }
        server_->send_message_.type = Message::kRead;
        server_->send_message_.arg.read.len = msg_offset;
        return server_->GetServerState(State::StateInit);
    }
}

FileSystemServer::FileSystemServer() {}

void FileSystemServer::Initialize() {
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image_, 0, 1);
    if (err) {
        Print("[ fs ] cannnot read volume image");
        exit(1);
    }

    // supports only FAT32 TODO:supports other fat
    if (boot_volume_image_.total_sectors_16 == 0) {
        bytes_per_cluster_ = boot_volume_image_.bytes_per_sector *
                             boot_volume_image_.sectors_per_cluster;

        fat_ = reinterpret_cast<uint32_t *>(
            new char[boot_volume_image_.bytes_per_sector]);
        cluster_buf_ =
            reinterpret_cast<uint32_t *>(new char[bytes_per_cluster_]);

        state_pool_.emplace_back(new ErrState(this));
        state_pool_.emplace_back(new InitState(this));
        state_pool_.emplace_back(new ExecFileState(this));
        state_pool_.emplace_back(new ExpandBufferState(this));
        state_pool_.emplace_back(new CopyToBufferState(this));
        state_pool_.emplace_back(new OpenState(this));
        state_pool_.emplace_back(new ReadState(this));

        state_ = GetServerState(State::StateInit);

        Print("[ fs ] ready\n");
    }
}

unsigned long FileSystemServer::GetFAT(unsigned long cluster) {
    unsigned long sector_offset =
        cluster / (boot_volume_image_.bytes_per_sector / sizeof(uint32_t));

    // copy only 1 block
    auto [ret, err] = SyscallReadVolumeImage(
        fat_, boot_volume_image_.reserved_sector_count + sector_offset, 1);

    unsigned long index =
        cluster - ((boot_volume_image_.bytes_per_sector / sizeof(uint32_t)) *
                   sector_offset);
    return index;
}

void FileSystemServer::UpdateFAT(unsigned long cluster) {
    unsigned long sector_offset =
        cluster / (boot_volume_image_.bytes_per_sector / sizeof(uint32_t));

    auto [ret, err] = SyscallCopyToVolumeImage(
        fat_, boot_volume_image_.reserved_sector_count + sector_offset, 1);
}

unsigned long FileSystemServer::NextCluster(unsigned long cluster) {
    auto index = GetFAT(cluster);
    uint32_t next = fat_[index];
    // end of cluster chain
    if (next >= 0x0ffffff8ul) {
        return 0x0ffffffflu;
    }
    return next;
}

uint32_t *FileSystemServer::ReadCluster(unsigned long cluster) {
    unsigned long sector_offset =
        boot_volume_image_.reserved_sector_count +
        boot_volume_image_.num_fats * boot_volume_image_.fat_size_32 +
        (cluster - 2) * boot_volume_image_.sectors_per_cluster;

    auto [ret, err] = SyscallReadVolumeImage(
        cluster_buf_, sector_offset, boot_volume_image_.sectors_per_cluster);
    if (err) {
        return nullptr;
    }

    return cluster_buf_;
}

void FileSystemServer::UpdateCluster(unsigned long cluster) {
    unsigned long sector_offset =
        boot_volume_image_.reserved_sector_count +
        boot_volume_image_.num_fats * boot_volume_image_.fat_size_32 +
        (cluster - 2) * boot_volume_image_.sectors_per_cluster;

    auto [ret, err] = SyscallCopyToVolumeImage(
        cluster_buf_, sector_offset, boot_volume_image_.sectors_per_cluster);
}

bool FileSystemServer::NameIsEqual(DirectoryEntry &entry, const char *name) {
    unsigned char name83[11];
    memset(name83, 0x20, sizeof(name83));

    int i = 0;
    int i83 = 0;
    for (; name[i] != 0 && i83 < sizeof(name83); ++i, ++i83) {
        if (name[i] == '.') {
            i83 = 7;
            continue;
        }
        name83[i83] = toupper(name[i]);
    }
    return memcmp(entry.name, name83, sizeof(name83)) == 0;
}

std::pair<const char *, bool> FileSystemServer::NextPathElement(
    const char *path, char *path_elem) {
    const char *next_slash = strchr(path, '/');
    if (next_slash == nullptr) {
        strcpy(path_elem, path);
        return {nullptr, false};
    }

    const auto elem_len = next_slash - path;
    strncpy(path_elem, path, elem_len);
    path_elem[elem_len] = '\0';
    return {&next_slash[1], true};
}

std::pair<DirectoryEntry *, bool> FileSystemServer::FindFile(
    const char *path, unsigned long directory_cluster) {
    if (path[0] == '/') {
        directory_cluster = boot_volume_image_.root_cluster;
        ++path;
    } else if (directory_cluster == 0) {
        directory_cluster = boot_volume_image_.root_cluster;
    }

    char path_elem[13];
    const auto [next_path, post_slash] = NextPathElement(path, path_elem);
    const bool path_last = next_path == nullptr || next_path[0] == '\0';

    while (directory_cluster != 0x0ffffffflu) {
        DirectoryEntry *dir =
            reinterpret_cast<DirectoryEntry *>(ReadCluster(directory_cluster));

        for (int i = 0; i < (boot_volume_image_.bytes_per_sector *
                             boot_volume_image_.sectors_per_cluster) /
                                sizeof(DirectoryEntry);
             ++i) {
            if (dir[i].name == 0x00) {
                goto not_found;
            } else if (!NameIsEqual(dir[i], path_elem)) {
                continue;
            }

            if (dir[i].attr == Attribute::kDirectory && !path_last) {
                return FindFile(next_path, dir[i].FirstCluster());
            } else {
                return {&dir[i], post_slash};
            }
        }
        directory_cluster = NextCluster(directory_cluster);
    }
not_found:
    return {nullptr, post_slash};
}

void FileSystemServer::ReadName(DirectoryEntry &entry, char *base, char *ext) {
    memcpy(base, &entry.name[0], 8);
    base[8] = 0;
    for (int i = 7; i >= 0 && base[i] == 0x20; --i) {
        base[i] = 0;
    }
    memcpy(ext, &entry.name[8], 3);
    ext[3] = 0;
    for (int i = 2; i >= 0 && ext[i] == 0x20; --i) {
        ext[i] = 0;
    }
}

std::pair<DirectoryEntry *, int> FileSystemServer::CreateFile(
    const char *path) {
    auto parent_dir_cluster = boot_volume_image_.root_cluster;
    const char *filename = path;

    if (const char *slash_pos = strrchr(path, '/')) {
        filename = &slash_pos[1];
        if (slash_pos[1] == '\0') {
            return {nullptr, EISDIR};
        }

        char parent_dir_name[slash_pos - path + 1];
        strncpy(parent_dir_name, path, slash_pos - path);
        parent_dir_name[slash_pos - path] = '\0';

        if (parent_dir_name[0] != '\0') {
            auto [parent_dir, post_slash2] = FindFile(parent_dir_name);
            if (parent_dir == nullptr) {
                return {nullptr, ENOENT};
            }
            parent_dir_cluster = parent_dir->FirstCluster();
        }
    }
    auto [dir, allocated_cluster] = AllocateEntry(parent_dir_cluster);
    if (dir == nullptr) {
        return {nullptr, ENOSPC};
    }
    SetFileName(*dir, filename);
    dir->file_size = 0;
    UpdateCluster(allocated_cluster);
    return {dir, 0};
}

std::pair<DirectoryEntry *, unsigned long> FileSystemServer::AllocateEntry(
    unsigned long dir_cluster) {
    while (true) {
        auto dir = reinterpret_cast<DirectoryEntry *>(ReadCluster(dir_cluster));
        for (int i = 0; i < bytes_per_cluster_ / sizeof(DirectoryEntry); ++i) {
            if (dir[i].name[0] == 0 || dir[i].name[0] == 0xe5) {
                return {&dir[i], dir_cluster};
            }
        }
        auto next = NextCluster(dir_cluster);
        if (next == 0x0ffffffflu) {
            break;
        }
        dir_cluster = next;
    }

    dir_cluster = ExtendCluster(dir_cluster, 1);
    auto dir = reinterpret_cast<DirectoryEntry *>(ReadCluster(dir_cluster));
    memset(dir, 0, bytes_per_cluster_);
    return {&dir[0], dir_cluster};
}

unsigned long FileSystemServer::ExtendCluster(unsigned long eoc_cluster,
                                              size_t n) {
    while (1) {
        auto index = GetFAT(eoc_cluster);
        eoc_cluster = fat_[index];
        if (eoc_cluster == 0x0ffffffflu) {
            break;
        }
    }

    size_t num_allocated = 0;
    auto current = eoc_cluster;

    for (unsigned long candidate = 2; num_allocated < n; ++candidate) {
        auto i = GetFAT(candidate);
        if (fat_[i] != 0) {
            continue;  // candidate cluster is not free
        }
        auto j = GetFAT(current);
        fat_[j] = candidate;
        UpdateFAT(current);
        current = candidate;
        ++num_allocated;
    }
    auto j = GetFAT(current);
    fat_[j] = 0x0ffffffflu;
    UpdateFAT(current);
    return current;
}

void FileSystemServer::SetFileName(DirectoryEntry &entry, const char *name) {
    const char *dot_pos = strrchr(name, '.');
    memset(entry.name, ' ', 8 + 3);
    if (dot_pos) {
        for (int i = 0; i < 8 && i < dot_pos - name; ++i) {
            entry.name[i] = toupper(name[i]);
        }
        for (int i = 0; i < 3 && dot_pos[i + 1]; ++i) {
            entry.name[8 + i] = toupper(dot_pos[i + 1]);
        }
    } else {
        for (int i = 0; i < 8 && name[i]; ++i) {
            entry.name[i] = toupper(name[i]);
        }
    }
}
