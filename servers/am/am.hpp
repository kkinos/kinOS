#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "../../libs/common/template.hpp"
#include "../../libs/mikanos/mikansyscall.hpp"

const int kRows = 15;
const int kColumns = 60;
const int Marginx = 4;
const int Marginy = 24;
const int kCanvasWidth = kColumns * 8 + 8;
const int kCanvasHeight = kRows * 16 + 8;
int cursorx = 0;
int cursory = 0;
const int kLineMax = 128;
int linebuf_index_{0};
Vector2D<int> kTopLeftMargin = {4, 24};

Vector2D<int> CalcCursorPos();

void Scroll1(uint64_t layer_id);
void Print(uint64_t layer_id, const char *s,
           std::optional<size_t> len = std::nullopt);
void Print(uint64_t layer_id, char s);
void PrintUserName(uint64_t layer_id, const char *s,
                   std::optional<size_t> len = std::nullopt);
void PrintUserName(uint64_t layer_id, char s);

int PrintToTerminal(uint64_t layer_id, const char *format, ...);

Message msg[1];
