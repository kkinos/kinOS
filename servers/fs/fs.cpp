#include "fs.hpp"

#include <errno.h>
#include <fcntl.h>

#include <algorithm>
#include <cctype>
#include <utility>

#include "../../libs/common/message.hpp"
#include "../../libs/kinos/common/print.hpp"
#include "../../libs/kinos/common/syscall.h"

FileSystemServer *server;

extern "C" void main() {
    server = new FileSystemServer;
    server->Initialize();
    while (true) {
        server->ReceiveMessage();
        server->HandleMessage();
        server->SendMessage();
    }
}

FileSystemServer::FileSystemServer() {}

void FileSystemServer::Initialize() {
    auto [ret, err1] = SyscallReadVolumeImage(&bpb_, 0, 1);
    if (err1) {
        Print("[ fs ] cannnot read volume image");
        exit(1);
    }

    auto [am_id, err2] = SyscallFindServer("servers/am");
    if (err2) {
        Print("[ am ] cannnot find file system server\n");
    }
    am_id_ = am_id;

    // supports only FAT32 TODO:supports other fat
    if (bpb_.total_sectors_16 == 0) {
        bytes_per_cluster_ = bpb_.bytes_per_sector * bpb_.sectors_per_cluster;

        fat_ = reinterpret_cast<uint32_t *>(new char[bpb_.bytes_per_sector]);
        cluster_buf_ =
            reinterpret_cast<uint32_t *>(new char[bytes_per_cluster_]);

        state_pool_.emplace_back(new ErrState(this));
        state_pool_.emplace_back(new InitState(this));
        state_pool_.emplace_back(new ExecFileState(this));
        state_pool_.emplace_back(new ExpandBufferState(this));
        state_pool_.emplace_back(new CopyToBufferState(this));
        state_pool_.emplace_back(new OpenState(this));
        state_pool_.emplace_back(new ReadState(this));
        state_pool_.emplace_back(new WriteState(this));

        state_ = GetServerState(State::StateInit);

        Print("[ fs ] ready\n");
    }
}

unsigned long FileSystemServer::GetFAT(unsigned long cluster) {
    unsigned long sector_offset =
        cluster / (bpb_.bytes_per_sector / sizeof(uint32_t));

    // copy only 1 block
    auto [ret, err] = SyscallReadVolumeImage(
        fat_, bpb_.reserved_sector_count + sector_offset, 1);

    unsigned long index =
        cluster - ((bpb_.bytes_per_sector / sizeof(uint32_t)) * sector_offset);
    return index;
}

void FileSystemServer::UpdateFAT(unsigned long cluster) {
    unsigned long sector_offset =
        cluster / (bpb_.bytes_per_sector / sizeof(uint32_t));

    auto [ret, err] = SyscallCopyToVolumeImage(
        fat_, bpb_.reserved_sector_count + sector_offset, 1);
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
    cluster_num_ = cluster;

    unsigned long sector_offset = bpb_.reserved_sector_count +
                                  bpb_.num_fats * bpb_.fat_size_32 +
                                  (cluster - 2) * bpb_.sectors_per_cluster;

    auto [ret, err] = SyscallReadVolumeImage(cluster_buf_, sector_offset,
                                             bpb_.sectors_per_cluster);
    if (err) {
        return nullptr;
    }

    return cluster_buf_;
}

void FileSystemServer::UpdateCluster(unsigned long cluster) {
    if (cluster == 0) {
        cluster = cluster_num_;
    }
    unsigned long sector_offset = bpb_.reserved_sector_count +
                                  bpb_.num_fats * bpb_.fat_size_32 +
                                  (cluster - 2) * bpb_.sectors_per_cluster;

    auto [ret, err] = SyscallCopyToVolumeImage(cluster_buf_, sector_offset,
                                               bpb_.sectors_per_cluster);
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
        directory_cluster = bpb_.root_cluster;
        ++path;
    } else if (directory_cluster == 0) {
        directory_cluster = bpb_.root_cluster;
    }

    char path_elem[13];
    const auto [next_path, post_slash] = NextPathElement(path, path_elem);
    const bool path_last = next_path == nullptr || next_path[0] == '\0';

    while (directory_cluster != 0x0ffffffflu) {
        DirectoryEntry *dir =
            reinterpret_cast<DirectoryEntry *>(ReadCluster(directory_cluster));

        for (int i = 0; i < (bpb_.bytes_per_sector * bpb_.sectors_per_cluster) /
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
    auto parent_dir_cluster = bpb_.root_cluster;
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
        if (fat_[index] >= 0x0ffffff8ul) {
            break;
        }
        eoc_cluster = fat_[index];
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

unsigned long FileSystemServer::AllocateClusterChain(size_t n) {
    unsigned long first_cluster;
    for (first_cluster = 2;; ++first_cluster) {
        auto i = GetFAT(first_cluster);
        if (fat_[i] == 0) {
            fat_[i] = 0x0ffffffflu;
            break;
        }
    }
    UpdateFAT(first_cluster);

    if (n > 1) {
        ExtendCluster(first_cluster, n - 1);
    }
    return first_cluster;
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
