#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <optional>


#include "../../libs/common/template.hpp"
#include "../../libs/kinos/exec.hpp"
#include "../../libs/mikanos/mikansyscall.hpp"


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
std::array<char, kLineMax> linebuf_{};
char buffer[kRows][kColumns + 1];
Vector2D<int> kTopLeftMargin = {4, 24};

Vector2D<int> CalcCursorPos();
void DrawCursor(uint64_t layer_id, bool visible);
Rectangle<int> BlinkCursor(uint64_t layer_id);
Rectangle<int> InputKey(uint64_t layer_id, uint8_t modifier, uint8_t keycode, char ascii);
void Scroll1(uint64_t layer_id);
void Print(uint64_t layer_id, const char* s, std::optional<size_t> len = std::nullopt);
void Print(uint64_t layer_id, char s);
void PrintUserName(uint64_t layer_id, const char* s, std::optional<size_t> len = std::nullopt);
void PrintUserName(uint64_t layer_id, char s);
void ExecuteLine(uint64_t layer_id);

int PrintToTerminal(uint64_t layer_id, const char* format, ...);


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