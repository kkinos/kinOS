#include "fs.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "../../libs/kinos/print.hpp"

FileSystemServer::FileSystemServer() {}

void FileSystemServer::Initilaize() {
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image_, 0, 1);
    if (err) {
        Print("[ fs ] cannnot read volume image");
        ChangeState(State::Error);
    }

    // supports only FAT32 TODO:supports other fat
    if (boot_volume_image_.total_sectors_16 == 0) {
        fat_ = reinterpret_cast<uint32_t *>(new char[SECTOR_SIZE]);
        file_buf_ = reinterpret_cast<uint32_t *>(
            new char[boot_volume_image_.sectors_per_cluster * SECTOR_SIZE]);

        ChangeState(State::InitialState);
        Print("[ fs ] ready\n");
    }
}

void FileSystemServer::Processing() {
    // TODO add case State::Error
    switch (state_) {
        case State::InitialState: {
            ReceiveMessage();
            switch (received_message_.type) {
                case Message::kError: {
                    if (received_message_.arg.error.retry) {
                        SyscallSendMessage(&send_message_, am_id_);
                        Print("[ fs ] retry\n");
                    } else {
                        Print("[ fs ] error at am server\n");
                    }
                } break;

                case Message::kExecuteFile: {
                    ChangeState(State::ExecuteFile);
                    FindFile();
                } break;

                case Message::kOpen: {
                    ChangeState(State::OpenFile);
                    FindFile();
                } break;

                default:
                    Print("[ fs ] unknown message type \n");
                    break;
            }
        } break;

        case State::ExecuteFile: {
            ReceiveMessage();
            switch (received_message_.type) {
                case Message::kError: {
                    if (received_message_.arg.error.retry) {
                        SyscallSendMessage(&send_message_, am_id_);
                        Print("[ fs ] retry\n");
                    } else {
                        Print("[ fs ] error at am server\n");
                        ChangeState(State::InitialState);
                    }
                } break;

                case Message::kExecuteFile: {
                    target_task_id_ = received_message_.arg.executefile.id;
                    send_message_.type = Message::kExpandTaskBuffer;
                    send_message_.arg.expand.id = target_task_id_;
                    send_message_.arg.expand.bytes =
                        target_file_entry_->file_size;
                    SyscallSendMessage(&send_message_, 1);
                    ChangeState(State::CopyFileToTaskBuffer);
                } break;

                default:
                    Print("[ fs ] unknown message type from am server\n");
                    ChangeState(State::InitialState);
                    break;
            }
        } break;

        case State::CopyFileToTaskBuffer: {
            ReceiveMessage();
            switch (received_message_.type) {
                case Message::kError: {
                    if (received_message_.arg.error.retry) {
                        SyscallSendMessage(&send_message_, 1);
                        Print("[ fs ] retry\n");
                    } else {
                        Print("[ fs ] error at kernel\n");
                        ChangeState(State::InitialState);
                    }
                } break;

                case Message::kExpandTaskBuffer: {
                    auto cluster = target_file_entry_->FirstCluster();
                    auto remain_bytes = target_file_entry_->file_size;
                    int offset = 0;
                    Print("[ fs ] copy %s to task buffer\n",
                          target_file_entry_->name);

                    while (cluster != 0 && cluster != 0x0ffffffflu) {
                        char *p =
                            reinterpret_cast<char *>(ReadCluster(cluster));
                        int i = 0;
                        for (; i < boot_volume_image_.bytes_per_sector *
                                       boot_volume_image_.sectors_per_cluster &&
                               i < remain_bytes;
                             ++i) {
                            auto [res, err] = SyscallCopyToTaskBuffer(
                                target_task_id_, p, offset, 1);
                            ++p;
                            ++offset;
                        }
                        remain_bytes -= i;
                        cluster = NextCluster(cluster);
                    }
                    send_message_.type = Message::kReady;
                    SyscallSendMessage(&send_message_, am_id_);
                    ChangeState(State::InitialState);
                } break;

                default:
                    Print("[ fs ] Unknown message type from kernel \n");
                    break;
            }
        } break;

        default:
            break;
    }
}

void FileSystemServer::ReceiveMessage() {
    switch (state_) {
        case State::InitialState:
        case State::ExecuteFile: {
            auto [am_id, err] = SyscallFindServer("servers/am");
            if (err) {
                Print(
                    "[ fs ] cannot find file application management server\n");
                ChangeState(State::Error);
                break;
            }

            am_id_ = am_id;
            SyscallClosedReceiveMessage(&received_message_, 1, am_id_);

        } break;

        case State::CopyFileToTaskBuffer: {
            SyscallClosedReceiveMessage(&received_message_, 1, 1);
        } break;

        default:
            break;
    }
}

void FileSystemServer::FindFile() {
    switch (state_) {
        case State::ExecuteFile: {
            const char *path = received_message_.arg.executefile.filename;
            Print("[ fs ] find  %s\n", path);

            auto [file_entry, post_slash] = FindFile(path);
            // the file doesn't exist
            if (!file_entry) {
                send_message_.type = Message::kExecuteFile;
                send_message_.arg.executefile.exist = false;
                send_message_.arg.executefile.isdirectory = false;
                SyscallSendMessage(&send_message_, am_id_);
                Print("[ fs ] cannnot find  %s\n", path);
                ChangeState(State::InitialState);
            }

            // the file is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                send_message_.type = Message::kExecuteFile;
                send_message_.arg.executefile.exist = true;
                send_message_.arg.executefile.isdirectory = true;
                SyscallSendMessage(&send_message_, am_id_);
                ChangeState(State::InitialState);
            }

            // the file exists and not a directory
            else {
                target_file_entry_ = file_entry;
                send_message_.type = Message::kExecuteFile;
                send_message_.arg.executefile.exist = true;
                send_message_.arg.executefile.isdirectory = false;
                SyscallSendMessage(&send_message_, am_id_);
            }
        } break;

        case State::OpenFile: {
            const char *path = received_message_.arg.open.filename;
            Print("[ fs ] find  %s\n", path);
            auto [file_entry, post_slash] = FindFile(path);

            // the file doesn't exist
            if (!file_entry) {
                send_message_.type = Message::kOpen;
                send_message_.arg.open.exist = false;
                send_message_.arg.open.isdirectory = false;
                Print("[ fs ] cannnot find  %s\n", path);
                SyscallSendMessage(&send_message_, am_id_);
            }

            // the file is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                send_message_.type = Message::kOpen;
                send_message_.arg.open.exist = true;
                send_message_.arg.open.isdirectory = true;
                SyscallSendMessage(&send_message_, am_id_);
            }
            // the file exists and not a directory
            else {
                send_message_.type = Message::kOpen;
                send_message_.arg.open.exist = true;
                send_message_.arg.open.isdirectory = false;
                SyscallSendMessage(&send_message_, am_id_);
            }
            ChangeState(State::InitialState);
        } break;

        default:
            break;
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

extern "C" void main() {
    file_system_server = new FileSystemServer;
    file_system_server->Initilaize();

    while (true) {
        file_system_server->Processing();
    }
}