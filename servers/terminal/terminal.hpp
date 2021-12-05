#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <optional>

#include "../../libs/common/template.hpp"
#include "../../libs/kinos/app/gui/guisyscall.hpp"

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
Vector2D<int> kTopLeftMargin = {4, 24};
std::deque<std::array<char, kLineMax>> cmd_history_{};
int cmd_history_index_{-1};

int last_exit_code_{0};
char* subcommand;
uint64_t waiting_task_id_;

static const int kLControlBitMask = 0b00000001u;
static const int kRControlBitMask = 0b00010000u;

Message send_message[1];
Message received_message[1];

Vector2D<int> CalcCursorPos();
void DrawCursor(uint64_t layer_id, bool visible);
Rectangle<int> BlinkCursor(uint64_t layer_id);

int PrintT(uint64_t layer_id, const char* format, ...);
void Scroll1(uint64_t layer_id);
void Print(uint64_t layer_id, const char* s,
           std::optional<size_t> len = std::nullopt);
void Print(uint64_t layer_id, char s);
void PrintInGreen(uint64_t layer_id, const char* s,
                  std::optional<size_t> len = std::nullopt);
void PrintInGreen(uint64_t layer_id, char s);

Rectangle<int> HistoryUpDown(int direction, uint64_t layer_id);
Rectangle<int> InputKey(uint64_t layer_id, uint8_t modifier, uint8_t keycode,
                        char ascii);

void ExecuteLine(uint64_t layer_id);
void ExecuteFile(uint64_t layer_id, char* line);
