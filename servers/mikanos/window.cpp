/**
 * @file window.hpp
 *
 * 表示領域を表すWindowクラスを提供する。
 */

#include "window.hpp"

Window::Window(int width, int height) : width_{width}, height_{height} {
    data_.resize(height);
    for (int y = 0; y < height; ++y) {
        data_[y].resize(width);
    }
    /**
    FrameBufferConfig config{};
    config.frame_buffer = nullptr;
    config.horizontal_resolution = width;
    config.vertical_resolution = height;
    config.pixel_format = shadow_format;

    if (auto err = shadow_buffer_.Initialize(config)) {
        Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }
    */
}

void Window::DrawTo(PixelWriter& writer, Vector2D<int> position) {
    if (!transparent_color_) {
        for (int y = 0; y < Height(); ++y) {
            for (int x = 0; x < Width(); ++ x) {
                writer.Write(position + Vector2D<int>{x, y}, At({ x, y }));
            }
        }
        return;
    }

    const auto tc = transparent_color_.value();
    for (int y = 0; y < Height(); ++y) {
        for (int x = 0; x < Width(); ++ x) {
            const auto c = At({ x, y });
            if (c != tc) {
                writer.Write(position + Vector2D<int>{x, y}, c);
            }
        }
    }
}

void Window::SetTransparentColor(std::optional<PixelColor> c) {
    transparent_color_ = c;
}

Window::WindowWriter* Window::Writer() {
    return &writer_;
}


const PixelColor& Window::At(Vector2D<int> pos) const{
    return data_[pos.y][pos.x];
}

PixelColor& Window::At(Vector2D<int> pos) {
  return data_[pos.y][pos.x];
}

int Window::Width() const {
    return width_;
}

int Window::Height() const {
    return height_;
}