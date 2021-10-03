/**
 * @file graphics.cpp
 *
 * グラフィック周りのファイル
 */

#include "graphics.hpp"

#include "font.hpp"
void FrameBufferWriter::Write(Vector2D<int> pos, const PixelColor& c) {
    SyscallWritePixel(pos.x, pos.y, c.r, c.g, c.b);
}

int FrameBufferWriter::Width() const {
    auto [w, err] = SyscallFrameBufferWidth();
    if (err) {
        return 0;
    }
    int width = static_cast<int>(w);
    return width;
}

int FrameBufferWriter::Height() const {
    auto [h, err] = SyscallFrameBufferHeight();
    if (err) {
        return 0;
    }
    int height = static_cast<int>(h);
    return height;
}

void ShadowBufferWriter::Write(Vector2D<int> pos, const PixelColor& c) {
    auto p = PixelAt(pos);
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
}

/**
 * @brief 四角形を描写する
 *
 * @param pos
 * @param size
 * @param c
 */
void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c) {
    for (int dx = 0; dx < size.x; ++dx) {
        writer.Write(pos + Vector2D<int>{dx, 0}, c);
        writer.Write(pos + Vector2D<int>{dx, size.y - 1}, c);
    }
    for (int dy = 1; dy < size.y - 1; ++dy) {
        writer.Write(pos + Vector2D<int>{0, dy}, c);
        writer.Write(pos + Vector2D<int>{size.x - 1, dy}, c);
    }
}

/**
 * @brief 四角形を中を埋めた状態で表示する
 *
 * @param pos
 * @param size
 * @param c
 */
void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c) {
    for (int dy = 0; dy < size.y; ++dy) {
        for (int dx = 0; dx < size.x; ++dx) {
            writer.Write(pos + Vector2D<int>{dx, dy}, c);
        }
    }
}

PixelWriter* screen_writer;

void DrawDesktop(PixelWriter& writer) {
    const auto width = writer.Width();
    const auto height = writer.Height();

    FillRectangle(writer, {0, 0}, {width, height - 50}, kDesktopBGColor);

    FillRectangle(writer, {0, height - 50}, {width, 50}, {1, 8, 17});

    FillRectangle(writer, {0, height - 50}, {width / 5, 50}, {80, 80, 80});

    DrawRectangle(writer, {10, height - 40}, {30, 30}, {160, 160, 160});
}

Vector2D<int> ScreenSize() {
    auto [w, errw] = SyscallFrameBufferWidth();
    if (errw) {
        return {0, 0};
    }
    int width = static_cast<int>(w);

    auto [h, errh] = SyscallFrameBufferHeight();
    if (errh) {
        return {0, 0};
    }
    int height = static_cast<int>(h);
    return {width, height};
}

namespace {
char pixel_writer_buf[sizeof(FrameBufferWriter)];
}

void InitializeGraphics() {
    screen_writer = new (pixel_writer_buf) FrameBufferWriter;
    DrawDesktop(*screen_writer);
}
