#include "fs.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "../../libs/kinos/print.hpp"

extern "C" void main() {
    server = new FileSystemServer;
    server->Initilaize();
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
                return server_->GetServerState(State::StateExec);
            } break;

            case Message::kOpen: {
                return server_->GetServerState(State::StateOpen);
            } break;

            case Message::kOpenDir: {
                return server_->GetServerState(State::StateOpenDir);
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

ExecState::ExecState(FileSystemServer *server) : server_{server} {}

ServerState *ExecState::ReceiveMessage() {
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
                return server_->GetServerState(State::StateExpand);
            }
            default:
                Print("[ fs ] unknown message from am server\n");
                return server_->GetServerState(State::StateErr);
        }
    }
}

ServerState *ExecState::HandleMessage() {
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

ServerState *ExecState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->am_id_);
    return this;
}

ExpandState::ExpandState(FileSystemServer *server) : server_{server} {}

ServerState *ExpandState::ReceiveMessage() {
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
                return server_->GetServerState(State::StateCopy);
            }
            default:
                Print("[ fs ] unknown message from am server");
                return server_->GetServerState(State::StateErr);
        }
    }
}
ServerState *ExpandState::HandleMessage() {
    server_->target_task_id_ = server_->received_message_.arg.executefile.id;
    server_->send_message_.type = Message::kExpandTaskBuffer;
    server_->send_message_.arg.expand.id = server_->target_task_id_;
    server_->send_message_.arg.expand.bytes =
        server_->target_file_entry_->file_size;
    return this;
}

ServerState *ExpandState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, 1);
    return this;
}

CopyState::CopyState(FileSystemServer *server) : server_{server} {}

ServerState *CopyState::HandleMessage() {
    auto cluster = server->target_file_entry_->FirstCluster();
    auto remain_bytes = server->target_file_entry_->file_size;
    int offset = 0;
    Print("[ fs ] copy %s to task buffer\n", server->target_file_entry_->name);

    while (cluster != 0 && cluster != 0x0ffffffflu) {
        char *p = reinterpret_cast<char *>(server->ReadCluster(cluster));
        int i = 0;
        for (; i < server->boot_volume_image_.bytes_per_sector *
                       server->boot_volume_image_.sectors_per_cluster &&
               i < remain_bytes;
             ++i) {
            auto [res, err] =
                SyscallCopyToTaskBuffer(server->target_task_id_, p, offset, 1);
            ++p;
            ++offset;
        }
        remain_bytes -= i;
        cluster = server->NextCluster(cluster);
    }
    server->send_message_.type = Message::kReady;
    return this;
}

ServerState *CopyState::SendMessage() {
    SyscallSendMessage(&server_->send_message_, server_->am_id_);
    return server_->GetServerState(State::StateInit);
}

OpenState::OpenState(FileSystemServer *server) : server_{server} {}

ServerState *OpenState::HandleMessage() {
    const char *path = server_->received_message_.arg.open.filename;
    Print("[ fs ] find  %s\n", path);
    auto [file_entry, post_slash] = server_->FindFile(path);
    // the file doesn't exist
    if (!file_entry) {
        server_->send_message_.type = Message::kOpen;
        server_->send_message_.arg.open.exist = false;
        server_->send_message_.arg.open.isdirectory = false;
        Print("[ fs ] cannnot find  %s\n", path);
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
}

OpenDirState::OpenDirState(FileSystemServer *server) : server_{server} {}

ServerState *OpenDirState::HandleMessage() {
    const char *path = server_->received_message_.arg.opendir.dirname;
    Print("[ fs ] find  %s\n", path);
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
    return server_->GetServerState(State::StateInit);
}

ReadState::ReadState(FileSystemServer *server) : server_{server} {}

ServerState *ReadState::HandleMessage() {
    const char *path = server_->received_message_.arg.read.filename;
    auto [file_entry, post_slash] = server_->FindFile(path);

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
        for (; i < server_->boot_volume_image_.bytes_per_sector *
                       server_->boot_volume_image_.sectors_per_cluster &&
               i < remain_bytes && sent_bytes < count;
             ++i) {
            if (total >= read_offset) {
                memcpy(&server_->send_message_.arg.read.data[msg_offset], p, 1);
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

FileSystemServer::FileSystemServer() {}

void FileSystemServer::Initilaize() {
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image_, 0, 1);
    if (err) {
        Print("[ fs ] cannnot read volume image");
        exit(1);
    }

    // supports only FAT32 TODO:supports other fat
    if (boot_volume_image_.total_sectors_16 == 0) {
        fat_ = reinterpret_cast<uint32_t *>(new char[SECTOR_SIZE]);
        file_buf_ = reinterpret_cast<uint32_t *>(
            new char[boot_volume_image_.sectors_per_cluster * SECTOR_SIZE]);

        state_pool_.emplace_back(new ErrState(this));
        state_pool_.emplace_back(new InitState(this));
        state_pool_.emplace_back(new ExecState(this));
        state_pool_.emplace_back(new ExpandState(this));
        state_pool_.emplace_back(new CopyState(this));
        state_pool_.emplace_back(new OpenState(this));
        state_pool_.emplace_back(new OpenDirState(this));
        state_pool_.emplace_back(new ReadState(this));

        state_ = GetServerState(State::StateInit);

        Print("[ fs ] ready\n");
    }
}

unsigned long FileSystemServer::NextCluster(unsigned long cluster) {
    unsigned long sector_offset =
        cluster / (boot_volume_image_.bytes_per_sector / sizeof(uint32_t));

    // copy 1 block
    auto [ret, err] = SyscallReadVolumeImage(
        fat_, boot_volume_image_.reserved_sector_count + sector_offset, 1);
    if (err) {
        return 0;
    }

    cluster =
        cluster - ((boot_volume_image_.bytes_per_sector / sizeof(uint32_t)) *
                   sector_offset);
    uint32_t next = fat_[cluster];

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
        file_buf_, sector_offset, boot_volume_image_.sectors_per_cluster);
    if (err) {
        return nullptr;
    }

    return file_buf_;
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
