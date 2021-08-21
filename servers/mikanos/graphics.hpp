#pragma once

#include <cstdlib>

#include "../../libs/kinos/syscall.h"
#include "../../libs/common/template.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

inline bool operator==(const PixelColor& lhs, const PixelColor& rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator!=(const PixelColor& lhs, const PixelColor& rhs) {
  return !(lhs == rhs);
}

class PixelWriter {
 public:
  virtual ~PixelWriter() = default;
  virtual void Write(Vector2D<int> pos, const PixelColor& c) = 0;
  virtual int Width() const = 0;
  virtual int Height() const = 0;
};

class FrameBufferWriter : public PixelWriter {
 public:
  FrameBufferWriter(){};
  virtual ~FrameBufferWriter() = default;
  virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
  virtual int Width() const override;
  virtual int Height() const override;

};



const PixelColor kDesktopBGColor{0, 172, 150};
const PixelColor kDesktopFGColor{255, 255, 255};
const PixelColor kDesktopTNColor{0, 255, 0};


void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

void DrawDesktop(PixelWriter& writer);

Vector2D<int> ScreenSize();
extern PixelWriter* screen_writer;

void InitializeGraphics();
