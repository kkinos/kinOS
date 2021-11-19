/**
 * @file guisyscall.hpp
 *
 * system calls provided by gui server
 */

#pragma once

#include "../../../common/message.hpp"
#include "../../syscall.h"

int OpenWindow(int w, int h, int x, int y, char* title);
void WinFillRectangle(int layer_id, bool draw, int x, int y, int w, int h,
                      uint32_t color);
void WinWriteChar(int layer_id, bool draw, int x, int y, uint32_t color,
                  char c);
void WinWriteString(int layer_id, bool draw, int x, int y, uint32_t color,
                    char* s);
void WinDrawLine(int layer_id, bool draw, int x0, int y0, int x1, int y1,
                 uint32_t color);
void WinMoveRec(int layer_id, bool draw, int x0, int y0, int rx0, int ry0,
                int rx1, int ry1);
void WinRedraw(int layer_id);
void CloseWindow(int layer_id);
