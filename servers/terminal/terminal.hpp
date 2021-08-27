#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <optional>


#include "../../libs/common/template.hpp"
#include "../../libs/kinos/exec.hpp"
#include "../../libs/mikanos/syscall.hpp"


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
void ExecuteLine(uint64_t layer_id);