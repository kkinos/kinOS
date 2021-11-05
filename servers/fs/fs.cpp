#include "fs.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

FileSystemServer::FileSystemServer() {}

void FileSystemServer::InitilaizeFat() {
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image_, 0, 1);
    if (err) {
        SyscallWriteKernelLog("[ fs ] cannnot read volume image");
    }

    // supports only FAT32 TODO:supports other fat
    if (boot_volume_image_.total_sectors_16 == 0) {
        fat_ = reinterpret_cast<uint32_t *>(new char[SECTOR_SIZE]);
        file_buf_ = reinterpret_cast<uint32_t *>(
            new char[boot_volume_image_.sectors_per_cluster * SECTOR_SIZE]);
    }
    ChangeState(State::WaitingForMessage);
}

void FileSystemServer::Processing() {
    // TODO add case State::HavingError
    switch (state_) {
        case State::WaitingForMessage: {
            auto [am_id, err] = SyscallFindServer("servers/am");
            if (err) {
                SyscallWriteKernelLog(
                    "[ fs ] cannot find file application management server\n");
                ChangeState(State::HavingError);
                break;
            }

            am_id_ = am_id;

            SyscallClosedReceiveMessage(&received_message_, 1, am_id_);

            if (received_message_.type == Message::kError) {
                if (received_message_.arg.error.retry) {
                    SyscallSendMessage(&send_message_, am_id_);
                    SyscallWriteKernelLog("[ fs ] retry\n");
                } else {
                    SyscallWriteKernelLog("[ fs ] error at am server\n");
                }

            } else if (received_message_.type == Message::kExecuteFile) {
                ChangeState(State::ExecutingFile);
                SearchFile();

            } else if (received_message_.type == Message::kOpen) {
                ChangeState(State::OpeningFile);
                SearchFile();

            } else if (received_message_.type ==
                       Message::kCopyFileToTaskBuffer) {
                ChangeState(State::CopyingFileToTaskBuffer);

            } else {
                SyscallWriteKernelLog("[ fs ] Unknown message type \n");
            }
        } break;

        case State::CopyingFileToTaskBuffer: {
        } break;

        default:
            break;
    }
}

void FileSystemServer::SearchFile() {
    switch (state_) {
        case State::ExecutingFile: {
            const char *path = received_message_.arg.executefile.filename;

            auto [file_entry, post_slash] = FindFile(path);
            // the file doesn't exist
            if (!file_entry) {
                send_message_.type = Message::kExecuteFile;
                send_message_.arg.executefile.exist = false;
                send_message_.arg.executefile.directory = false;
                SyscallSendMessage(&send_message_, am_id_);
            }

            // the file is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                send_message_.type = Message::kExecuteFile;
                send_message_.arg.executefile.exist = true;
                send_message_.arg.executefile.directory = true;
                SyscallSendMessage(&send_message_, am_id_);
            }

            // the file exists and not a directory
            else {
                send_message_.type = Message::kExecuteFile;
                send_message_.arg.executefile.exist = true;
                send_message_.arg.executefile.directory = false;
                SyscallSendMessage(&send_message_, am_id_);
            }

            ChangeState(State::WaitingForMessage);

        } break;

        case State::OpeningFile: {
            const char *path = received_message_.arg.open.filename;
            auto [file_entry, post_slash] = FindFile(path);

            // the file doesn't exist
            if (!file_entry) {
                send_message_.type = Message::kOpen;
                send_message_.arg.open.exist = false;
                send_message_.arg.open.directory = false;
                SyscallSendMessage(&send_message_, am_id_);
            }

            // the file is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                send_message_.type = Message::kOpen;
                send_message_.arg.open.exist = true;
                send_message_.arg.open.directory = true;
                SyscallSendMessage(&send_message_, am_id_);
            }
            // the file exists and not a directory
            else {
                send_message_.type = Message::kOpen;
                send_message_.arg.open.exist = true;
                send_message_.arg.open.directory = false;
                SyscallSendMessage(&send_message_, am_id_);
            }

            ChangeState(State::WaitingForMessage);
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

// void ProcessAccordingToMessage(uint64_t am_id) {
//     switch (received_message[0].type) {
//         case Message::kExecuteFile: {
//             const char *path =
//             received_message[0].arg.executefile.filename;

//             auto [file_entry, post_slash] = FindFile(path);
//             // the file doesn't exist
//             if (!file_entry) {
//                 sent_message[0].type = Message::kExecuteFile;
//                 sent_message[0].arg.executefile.exist = false;
//                 sent_message[0].arg.executefile.directory = false;
//                 SyscallSendMessage(sent_message, am_id);
//                 goto end;
//             }
//             // the file is a directory
//             else if (file_entry->attr == Attribute::kDirectory) {
//                 sent_message[0].type = Message::kExecuteFile;
//                 sent_message[0].arg.executefile.exist = true;
//                 sent_message[0].arg.executefile.directory = true;
//                 SyscallSendMessage(sent_message, am_id);
//                 goto end;
//             }
//             // the file exists and not a directory
//             else {
//                 sent_message[0].type = Message::kExecuteFile;
//                 sent_message[0].arg.executefile.exist = true;
//                 sent_message[0].arg.executefile.directory = false;
//                 SyscallSendMessage(sent_message, am_id);
//                 ExpandTaskBuffer(am_id, file_entry);
//                 goto end;
//             }
//         }

//         case Message::kOpen: {
//             const char *path = received_message[0].arg.open.filename;

//             auto [file_entry, post_slash] = FindFile(path);
//             // the file doesn't exist
//             if (!file_entry) {
//                 sent_message[0].type = Message::kOpen;
//                 sent_message[0].arg.open.exist = false;
//                 sent_message[0].arg.open.directory = false;
//                 SyscallSendMessage(sent_message, am_id);
//                 goto end;
//             }
//             // the file is a directory
//             else if (file_entry->attr == Attribute::kDirectory) {
//                 sent_message[0].type = Message::kOpen;
//                 sent_message[0].arg.open.exist = true;
//                 sent_message[0].arg.open.directory = true;
//                 SyscallSendMessage(sent_message, am_id);
//                 goto end;
//             }
//             // the file exists and not a directory
//             else {
//                 sent_message[0].type = Message::kOpen;
//                 sent_message[0].arg.open.exist = true;
//                 sent_message[0].arg.open.directory = false;
//                 SyscallSendMessage(sent_message, am_id);
//                 goto end;
//             }
//         }
//         default:
//             SyscallWriteKernelLog("[ fs ] Unknown message type \n");
//             goto end;
//     }

// end:
//     return;
// }

// void ExpandTaskBuffer(uint64_t am_id, DirectoryEntry *file_entry) {
//     while (true) {
//         SyscallClosedReceiveMessage(received_message, 1, am_id);
//         switch (received_message[0].type) {
//             case Message::kError:
//                 if (received_message[0].arg.error.retry) {
//                     SyscallSendMessage(sent_message, am_id);
//                     SyscallWriteKernelLog("[ fs ] retry\n");
//                     break;
//                 } else {
//                     SyscallWriteKernelLog("[ fs ] error at am server\n");
//                     goto end;
//                 }
//             case Message::kExecuteFile: {
//                 // message to kernel to expand task buffer
//                 uint64_t id = received_message[0].arg.executefile.id;
//                 sent_message[0].type = Message::kExpandTaskBuffer;
//                 sent_message[0].arg.expand.id = id;
//                 sent_message[0].arg.expand.bytes = file_entry->file_size;
//                 SyscallSendMessage(sent_message, 1);
//                 CopyToTaskBuffer(id, am_id, file_entry);
//                 goto end;
//             }

//             default:
//                 SyscallWriteKernelLog(
//                     "[ fs ] Unknown message type from am server\n");
//                 goto end;
//         }
//     }
// end:
//     return;
// }

// void CopyToTaskBuffer(uint64_t id, uint64_t am_id, DirectoryEntry
// *file_entry) {
//     while (true) {
//         SyscallClosedReceiveMessage(received_message, 1, 1);

//         switch (received_message[0].type) {
//             case Message::kError:
//                 if (received_message[0].arg.error.retry) {
//                     SyscallSendMessage(sent_message, 1);
//                     SyscallWriteKernelLog("[ fs ] retry\n");
//                     break;
//                 } else {
//                     SyscallWriteKernelLog("[ fs ] error at kernel\n");
//                     goto end;
//                 }
//             // copy file to task buffer
//             case Message::kExpandTaskBuffer: {
//                 auto cluster = file_entry->FirstCluster();
//                 auto remain_bytes = file_entry->file_size;
//                 int offset = 0;

//                 while (cluster != 0 && cluster != 0x0ffffffflu) {
//                     char *p = reinterpret_cast<char
//                     *>(ReadCluster(cluster)); int i = 0; for (; i <
//                     boot_volume_image.bytes_per_sector *
//                                    boot_volume_image.sectors_per_cluster
//                                    &&
//                            i < remain_bytes;
//                          ++i) {
//                         auto [res, err] =
//                             SyscallCopyToTaskBuffer(id, p, offset, 1);
//                         if (err) {
//                             SyscallWriteKernelLog("[ fs ] Syscall
//                             error\n"); sent_message[0].type =
//                             Message::kError;
//                             sent_message[0].arg.error.retry = false;
//                             SyscallSendMessage(sent_message, am_id);
//                             goto end;
//                         }
//                         ++p;
//                         ++offset;
//                     }
//                     remain_bytes -= i;
//                     cluster = NextCluster(cluster);
//                 }
//                 sent_message[0].type = Message::kReady;
//                 SyscallSendMessage(sent_message, am_id);
//                 goto end;
//             }

//             default:
//                 goto end;
//         }
//     }
// end:
//     return;
// }

extern "C" void main() {
    file_system_server = new FileSystemServer;
    file_system_server->InitilaizeFat();
    SyscallWriteKernelLog("[ fs ] ready\n");

    while (true) {
        file_system_server->Processing();
    }
}