#include "fat.hpp"


#include "../../libs/mikanos/mikansyscall.hpp"


BPB boot_volume_image;
size_t root_dir_block;
DirectoryEntry* root_dir;
size_t entries_per_cluster;


Error InitializeFat(
    ) 
    {
        auto [r, err] = SyscallReadVolumeImage(&boot_volume_image, 0, 1);
        if (err) {
            return MAKE_ERROR(Error::kInvalidFile);
        }
        root_dir_block = boot_volume_image.reserved_sector_count + boot_volume_image.num_fats *
                        boot_volume_image.fat_size_32 + (boot_volume_image.root_cluster - 2) *
                        boot_volume_image.sectors_per_cluster;
        entries_per_cluster = boot_volume_image.bytes_per_sector / 
                            sizeof(DirectoryEntry) * boot_volume_image.sectors_per_cluster;

        return MAKE_ERROR(Error::kSuccess);
    }

void OpenRootDir (
    )
    {
        root_dir = reinterpret_cast<DirectoryEntry*>(new uint8_t[boot_volume_image.sectors_per_cluster * SECTOR_SIZE]);
        SyscallReadVolumeImage(root_dir, root_dir_block, boot_volume_image.sectors_per_cluster);

    }

void ReadName (
    DirectoryEntry& entry,
    char* base,
    char* ext
    )
    {
        memcpy(base, &entry.name[0], 8);
        base[8] = 0;
        for (int i = 7; i>= 0 && base[i] == 0x20; --i) {
            base[i] = 0;
        }
        memcpy(ext, &entry.name[8], 3);
        ext[3] = 0;
        for (int i = 2; i >= 0 && ext[i] == 0x20; --i) {
            ext[i] = 0;
        }
    }