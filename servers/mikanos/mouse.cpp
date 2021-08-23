/**
 * @file mouse.cpp
 *
 * マウス制御プログラム．
 */

#include <limits>
#include <memory>

#include "graphics.hpp"
#include "mouse.hpp"
#include "console.hpp"
#include "font.hpp"

namespace {
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
}

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
  for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
    for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
      if (mouse_cursor_shape[dy][dx] == '@') {
        pixel_writer->Write(position + Vector2D<int>{ dx, dy }, {0, 0, 0});
      } else if (mouse_cursor_shape[dy][dx] == '.') {
        pixel_writer->Write(position + Vector2D<int>{ dx, dy }, {255, 255, 255});
      } else {
        pixel_writer->Write(position + Vector2D<int>{ dx, dy }, kMouseTransparentColor);
      }
      }
    }
  }

Mouse::Mouse(unsigned int layer_id) : layer_id_{layer_id} {
}

void Mouse::SetPosition(Vector2D<int> position) {
  position_ = position;
  layer_manager->Move(layer_id_, position_);
}

void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  const auto oldpos = position_;
  auto newpos = position_ + Vector2D<int>{displacement_x, displacement_y};
  newpos = ElementMin(newpos, ScreenSize() + Vector2D<int>{-1, -1});
  position_ = ElementMax(newpos, {0, 0});

  layer_manager->Move(layer_id_, position_);
  layer_manager->Draw();
}

Mouse* mouse;

namespace {
  char mouse_buf[sizeof(Mouse)];
}

void InitializeMouse() {
 auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});
 
 auto mouse_layer_id = layer_manager->NewLayer()
  .SetWindow(mouse_window)
  .ID();
  

  mouse = new(mouse_buf)Mouse{mouse_layer_id};
  mouse->SetPosition({200, 200});
  layer_manager->UpDown(mouse->LayerID(), 1);
  
}

