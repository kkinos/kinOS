#include "window.hpp"
#include "console.hpp"
#include "font.hpp"

Window::Window(int width, int height) : width_{width}, height_{height} {
    data_.resize(height);
    for (int y = 0; y < height; ++y) {
        data_[y].resize(width);
    }
    ShadowBufferConfig config{};
    config.shadow_buffer = nullptr;
    config.horizontal_resolution = width;
    config.vertical_resolution = height;

    if (auto err = shadow_buffer_.Initialize(config)) {
        console->PutString("failed to initialize shadow buffer");
    }
}

void Window::DrawTo(PixelWriter& writer, Vector2D<int> position) {
    if (!transparent_color_) {
        shadow_buffer_.CopyToFrameBuffer(position);
        return;
    }
    const auto tc = transparent_color_.value();
    for (int y = 0; y < Height(); ++y) {
        for (int x = 0; x < Width(); ++ x) {
            const auto c = At(Vector2D<int>{x, y});
            if (c != tc) {
                writer.Write(position + Vector2D<int>{x, y}, c);
            }
        }
    }
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
    shadow_buffer_.Move(dst_pos, src);
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

void Window::Write(Vector2D<int> pos, PixelColor c) {
    data_[pos.y][pos.x] = c;
    shadow_buffer_.Writer().Write(pos, c);
}

int Window::Width() const {
    return width_;
}

int Window::Height() const {
    return height_;
}
