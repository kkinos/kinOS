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

unsigned int mouse_layer_id;

void InitializeMouse() {
 auto mouse_window = std::make_shared<Window>(
   kMouseCursorWidth, kMouseCursorHeight);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

 mouse_layer_id = layer_manager->NewLayer()
  .SetWindow(mouse_window)
  .ID();

  layer_manager->UpDown(mouse_layer_id, 1);
  layer_manager->Draw();

}

void MouseObserver(int displacement_x, int displacement_y) {
  layer_manager->MoveRelative(mouse_layer_id, Vector2D<int>{ displacement_x, displacement_y });
  layer_manager->Draw();
}
