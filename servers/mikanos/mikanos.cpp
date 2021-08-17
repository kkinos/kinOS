#include <cstdlib>

#include "../../libs/kinos/syscall.h"
#include "../../libs/common/template.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

void DrawRectangle(const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c) {
  for (int dx = 0; dx < size.x; ++dx) {
    Vector2D<int> p1 = pos + Vector2D<int>{dx, 0};
    SyscallWritePixel(p1.x, p1.y, c.r, c.g, c.b);
    Vector2D<int> p2 = pos + Vector2D<int>{dx, size.y - 1};
    SyscallWritePixel(p2.x, p2.y, c.r, c.g, c.b);
  }
  for (int dy = 1; dy < size.y - 1; ++dy) {
    Vector2D<int> p3 = pos + Vector2D<int>{0, dy};
    SyscallWritePixel(p3.x, p3.y, c.r, c.g, c.b);
    Vector2D<int> p4 = pos + Vector2D<int>{size.x - 1, dy}; 
    SyscallWritePixel(p4.x, p4.y, c.r, c.g, c.b);
  }
}

void FillRectangle(const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c) {
  for (int dy = 0; dy < size.y; ++dy) {
    for (int dx = 0; dx < size.x; ++dx) {
        Vector2D<int> p = pos + Vector2D<int>{dx, dy};
        SyscallWritePixel(p.x, p.y, c.r, c.g, c.b);
    }
  }
}

const PixelColor kDesktopBGColor{0, 218, 145};

void DrawDesktop() {
    auto [ w, errw ] = SyscallFrameBufferWidth();
    auto [ h, errh ] = SyscallFrameBufferHeight();

    int width = static_cast<int>(w);
    int height = static_cast<int>(h);


    FillRectangle(  {0, 0},
                    {width, height - 50},
                    kDesktopBGColor);

    FillRectangle(  {0, height - 50},
                    {width, 50},
                    {1, 8, 17});

    FillRectangle(  {0, height - 50},
                    {width / 5, 50},
                    {80, 80, 80});

    DrawRectangle(  {10, height - 40},
                    {30, 30},
                    {160, 160, 160});
}




extern "C" int main() {
    DrawDesktop();
    exit(0);
    
}