#pragma once

#include "graphics.hpp"
#include "layer.hpp"


const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const PixelColor kMouseTransparentColor{0, 0, 1};

extern unsigned int mouse_layer_id;

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position);

void InitializeMouse();
void MouseObserver(int displacement_x, int displacement_y);
