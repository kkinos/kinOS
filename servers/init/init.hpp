#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "../../libs/common/message.hpp"
#include "../../libs/common/template.hpp"

#define SECTOR_SIZE 512

struct BPB {
    uint8_t jump_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed));

enum class Attribute : uint8_t {
    kReadOnly = 0x01,
    kHidden = 0x02,
    kSystem = 0x04,
    kVolumeID = 0x08,
    kDirectory = 0x10,
    kArchive = 0x20,
    kLongName = 0x0f,
};

struct DirectoryEntry {
    unsigned char name[11];
    Attribute attr;
    uint8_t ntres;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;

    uint32_t FirstCluster() const {
        return first_cluster_low |
               (static_cast<uint32_t>(first_cluster_high) << 16);
    }
} __attribute__((packed));

int num_servers = 5;
char servers[][32] = {
    "servers/gui", "servers/log",      "servers/am",
    "servers/fs",  "servers/terminal",
};

class InitServer {
   public:
    InitServer();
    void Initilaize();

    void StartServers(const char *server_name);
    void WaitingForMessage();

   private:
    Message send_message_;
    Message received_message_;

    BPB boot_volume_image_;
    uint64_t target_task_id_;
    uint32_t *fat_;
    uint32_t *file_buf_;

    unsigned long NextCluster(unsigned long cluster);
    uint32_t *ReadCluster(unsigned long cluster);
    std::pair<const char *, bool> NextPathElement(const char *path,
                                                  char *path_elem);
    std::pair<DirectoryEntry *, bool> FindFile(
        const char *path, unsigned long directory_cluster = 0);
    bool NameIsEqual(DirectoryEntry &entry, const char *name);
    void ReadName(DirectoryEntry &root_dir, char *base, char *ext);
};

InitServer *init_server;