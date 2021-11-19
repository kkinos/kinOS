#include "init.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "../../libs/kinos/common/print.hpp"
#include "../../libs/kinos/common/syscall.h"

InitServer::InitServer() {}

void InitServer::Initilaize() {
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image_, 0, 1);
    if (err) {
        Print("[ init ] cannnot read volume image");
    }

    // supports only FAT32 TODO:supports other fat
    if (boot_volume_image_.total_sectors_16 == 0) {
        fat_ = reinterpret_cast<uint32_t *>(new char[SECTOR_SIZE]);
        file_buf_ = reinterpret_cast<uint32_t *>(
            new char[boot_volume_image_.sectors_per_cluster * SECTOR_SIZE]);

        Print("[ init ] ready\n");
    }
}

void InitServer::StartServers(const char *server_name) {
    auto [file_entry, post_slash] = FindFile(server_name);
    if (!file_entry) {
        Print("[ init ] cannnot find %s\n", server_name);
    } else {
        auto [id, err] = SyscallCreateNewTask();
        target_task_id_ = id;
        send_message_.type = Message::kExpandTaskBuffer;
        send_message_.arg.expand.id = target_task_id_;
        send_message_.arg.expand.bytes = file_entry->file_size;
        SyscallSendMessage(&send_message_, 1);
        SyscallClosedReceiveMessage(&received_message_, 1, 1);

        if (received_message_.type == Message::kExpandTaskBuffer) {
            auto cluster = file_entry->FirstCluster();
            auto remain_bytes = file_entry->file_size;
            int offset = 0;

            while (cluster != 0 && cluster != 0x0ffffffflu) {
                char *p = reinterpret_cast<char *>(ReadCluster(cluster));
                int i = 0;
                for (; i < boot_volume_image_.bytes_per_sector *
                               boot_volume_image_.sectors_per_cluster &&
                       i < remain_bytes;
                     ++i) {
                    auto [res, err] =
                        SyscallCopyToTaskBuffer(target_task_id_, p, offset, 1);
                    ++p;
                    ++offset;
                }
                remain_bytes -= i;
                cluster = NextCluster(cluster);
            }
            char bufc[32];
            strcpy(bufc, server_name);

            SyscallSetArgument(target_task_id_, bufc);

            send_message_.type = Message::kStartServer;
            send_message_.arg.starttask.id = target_task_id_;
            SyscallSendMessage(&send_message_, 1);
        }
    }
}

void InitServer::WaitingForMessage() {
    SyscallOpenReceiveMessage(&received_message_, 1);
    if (received_message_.type == Message::kExitServer) {
        Print("[ init ] restart %s\n", received_message_.arg.exitserver.name);
        StartServers(received_message_.arg.exitserver.name);
    }
}

unsigned long InitServer::NextCluster(unsigned long cluster) {
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

uint32_t *InitServer::ReadCluster(unsigned long cluster) {
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

bool InitServer::NameIsEqual(DirectoryEntry &entry, const char *name) {
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

std::pair<const char *, bool> InitServer::NextPathElement(const char *path,
                                                          char *path_elem) {
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

std::pair<DirectoryEntry *, bool> InitServer::FindFile(
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

void InitServer::ReadName(DirectoryEntry &entry, char *base, char *ext) {
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
    init_server = new InitServer;
    init_server->Initilaize();

    for (int i = 0; i < num_servers; ++i) {
        init_server->StartServers(servers[i]);
    }

    while (true) {
        init_server->WaitingForMessage();
    }
}