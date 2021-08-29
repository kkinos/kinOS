#include "graphics.hpp"

void RGBResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& c) {
  auto p = PixelAt(pos);
  p[0] = c.r;
  p[1] = c.g;
  p[2] = c.b;
}


void BGRResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& c) {
  auto p = PixelAt(pos);
  p[0] = c.b;
  p[1] = c.g;
  p[2] = c.r;
}

FrameBufferConfig screen_config;
PixelWriter* screen_writer;
FrameBuffer* screen;

namespace {
  char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
  char frame_buffer_buf[sizeof(FrameBuffer)];
}


void InitializeGraphics(const FrameBufferConfig& screen_config) {
  ::screen_config = screen_config;

  switch (screen_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      ::screen_writer = new(pixel_writer_buf)
        RGBResv8BitPerColorPixelWriter{screen_config};
      break;
    case kPixelBGRResv8BitPerColor:
      ::screen_writer = new(pixel_writer_buf)
        BGRResv8BitPerColorPixelWriter{screen_config};
      break;
    default:
      exit(1);
  }

}