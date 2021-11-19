#pragma once

#include <cstdlib>

#include "../../libs/common/template.hpp"
#include "../../libs/kinos/common/syscall.h"
#include "shadow_buffer_config.hpp"

struct PixelColor {
    uint8_t r, g, b;
};

constexpr PixelColor ToColor(uint32_t c) {
    return {static_cast<uint8_t>((c >> 16) & 0xff),
            static_cast<uint8_t>((c >> 8) & 0xff),
            static_cast<uint8_t>(c & 0xff)};
}

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

class ShadowBufferWriter : public FrameBufferWriter {
   public:
    ShadowBufferWriter(const ShadowBufferConfig& config) : config_{config} {}
    virtual ~ShadowBufferWriter() = default;
    virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
    virtual int Width() const override { return config_.horizontal_resolution; }
    virtual int Height() const override { return config_.vertical_resolution; }

   protected:
    uint8_t* PixelAt(Vector2D<int> pos) {
        return config_.shadow_buffer +
               4 * (config_.pixels_per_scan_line * pos.y + pos.x);
    }

   private:
    const ShadowBufferConfig& config_;
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
