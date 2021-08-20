/**
 * @file mouse.hpp
 *
 * マウス制御プログラム．
 */

#pragma once

#include "graphics.hpp"

namespace {
  const int kMouseCursorWidth = 15;
  const int kMouseCursorHeight = 24;
  const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
  };

   void DrawMouseCursor(Vector2D<int> position) {
    for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
      for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
        if (mouse_cursor_shape[dy][dx] == '@') {
          Vector2D<int> p1 = position + Vector2D<int>{dx, dy};
          SyscallWritePixel(p1.x, p1.y, 0, 0, 0);

        } else if (mouse_cursor_shape[dy][dx] == '.') {
          Vector2D<int> p2 = position + Vector2D<int>{dx, dy};
          SyscallWritePixel(p2.x, p2.y, 255, 255, 255);
        }
      }
    }
  }

  void EraseMouseCursor(Vector2D<int> position,
                        PixelColor erase_color) {
    for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
      for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
        if (mouse_cursor_shape[dy][dx] != ' ') {
          Vector2D<int> p = position + Vector2D<int>{dx, dy};
          SyscallWritePixel(p.x, p.y, erase_color.r, erase_color.g, erase_color.b);
        }
      }
    }
  }
}


class MouseCursor {
 public:
  MouseCursor(PixelColor erase_color,
              Vector2D<int> initial_position);
  void MoveRelative(Vector2D<int> displacement);

 private:
  PixelColor erase_color_;
  Vector2D<int> position_;
};

MouseCursor::MouseCursor(PixelColor erase_color,
                         Vector2D<int> initial_position)
    : erase_color_{erase_color},
      position_{initial_position} {
  DrawMouseCursor(position_);
}

void MouseCursor::MoveRelative(Vector2D<int> displacement) {
  EraseMouseCursor(position_, erase_color_);
  position_ += displacement;
  DrawMouseCursor(position_);
}

char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;



void InitializeMouse() {
 mouse_cursor = new(mouse_cursor_buf) MouseCursor{
    kDesktopBGColor, {300, 200}
  };
}

