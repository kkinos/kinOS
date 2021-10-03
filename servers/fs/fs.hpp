#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <optional>

#include "../../libs/common/template.hpp"
#include "../../libs/common/error.hpp"
#include "../../libs/mikanos/mikansyscall.hpp"

#define SECTOR_SIZE 512

const int kRows = 15;
const int kColumns = 60;
const int Marginx = 4;
const int Marginy = 24;
const int kCanvasWidth = kColumns * 8 + 8;
const int kCanvasHeight = kRows * 16 + 8;
int cursorx = 0;
int cursory = 0;
bool cursor_visible_ = true;
const int kLineMax = 128;
int linebuf_index_{0};
Vector2D<int> kTopLeftMargin = {4, 24};

Message msg[1];

Vector2D<int> CalcCursorPos();
void DrawCursor(uint64_t layer_id, bool visible);
Rectangle<int> BlinkCursor(uint64_t layer_id);
Rectangle<int> InputKey(uint64_t layer_id, uint8_t modifier, uint8_t keycode, char ascii);
void Scroll1(uint64_t layer_id);
void Print(uint64_t layer_id, const char *s, std::optional<size_t> len = std::nullopt);
void Print(uint64_t layer_id, char s);

int PrintToTerminal(uint64_t layer_id, const char *format, ...);

struct BPB
{
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

enum class Attribute : uint8_t
{
  kReadOnly = 0x01,
  kHidden = 0x02,
  kSystem = 0x04,
  kVolumeID = 0x08,
  kDirectory = 0x10,
  kArchive = 0x20,
  kLongName = 0x0f,
};

struct DirectoryEntry
{
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

  uint32_t FirstCluster() const
  {
    return first_cluster_low |
           (static_cast<uint32_t>(first_cluster_high) << 16);
  }
} __attribute__((packed));

BPB boot_volume_image;
char fat_buf[SECTOR_SIZE];
uint32_t *fat;
uint32_t *fat_file;

/**
 * @brief ファイルサーバの最初の処理 BPBを読み取りファイル操作に必要な容量を確保する
 * 
 * @return Error 
 */
Error InitializeFat();

/**
 * @brief クラスタ番号を受け取りFATを読んで次のクラスタ番号を取得 エラーは0
 * 
 * @param cluster 
 * @return unsigned long 
 */
unsigned long NextCluster(unsigned long cluster);

/**
 * @brief クラスタ番号から該当するクラスタを読む エラーはnullptr
 * 
 * @param cluster 
 * @return uint32_t* 
 */
uint32_t *ReadCluster(unsigned long cluster);

/**
 * @brief path文字列を先頭から/で区切ってpath_elemにコピー
 * 
 * @param path 
 * @param path_elem 
 * @return std::pair<const char *, bool> 
 */
std::pair<const char *, bool>
NextPathElement(const char *path, char *path_elem);

/**
 * @brief 名前と一致するファイルを探し、なければnullptrを返す
 * 
 * @param path 
 * @param directory_cluster デフォルトで0になっていて0の場合はルートディレクトリを探す
 * @return std::pair<DirectoryEntry *, bool> 
 */
std::pair<DirectoryEntry *, bool>
FindFile(const char *path, unsigned long directory_cluster = 0);

bool NameIsEqual(const DirectoryEntry &entry, const char *name);

void ReadName(DirectoryEntry &root_dir, char *base, char *ext);
