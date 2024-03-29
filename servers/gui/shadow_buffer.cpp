#include "shadow_buffer.hpp"

#include "font.hpp"

namespace {

int BytesPerPixel() { return 4; }

uint8_t* ShadowAddrAt(Vector2D<int> pos, const ShadowBufferConfig& config) {
    return config.shadow_buffer +
           BytesPerPixel() * (config.pixels_per_scan_line * pos.y + pos.x);
}

int BytesPerScanLine(const ShadowBufferConfig& config) {
    return BytesPerPixel() * config.pixels_per_scan_line;
}

Vector2D<int> ShadowBufferSize(const ShadowBufferConfig& config) {
    return {static_cast<int>(config.horizontal_resolution),
            static_cast<int>(config.vertical_resolution)};
}
}  // namespace

Error ShadowBuffer::Initialize(const ShadowBufferConfig& config) {
    config_ = config;

    const auto bytes_per_pixel = 4; /*1pixel = 4bytes*/
    buffer_.resize(bytes_per_pixel * config_.horizontal_resolution *
                   config_.vertical_resolution);
    config_.shadow_buffer = buffer_.data();
    config_.pixels_per_scan_line = config_.horizontal_resolution;
    writer_ = std::make_unique<ShadowBufferWriter>(config_);
    return MAKE_ERROR(Error::kSuccess);
}

Error ShadowBuffer::CopyToFrameBuffer(Vector2D<int> pos,
                                      const Rectangle<int>& area) {
    const auto bytes_per_pixel = BytesPerPixel();
    const auto frame_buffer_width = static_cast<int>(ScreenSize().x);
    const auto frame_buffer_height = static_cast<int>(ScreenSize().y);

    const Rectangle<int> copy_area_shifted{pos, area.size};
    const Rectangle<int> shadow_buffer_outline{pos - area.pos,
                                               ShadowBufferSize(config_)};
    const Rectangle<int> frame_buffer_outline{
        {0, 0}, {frame_buffer_width, frame_buffer_height}};

    const auto copy_area =
        frame_buffer_outline & shadow_buffer_outline & copy_area_shifted;
    const auto shadow_buf_start_pos = copy_area.pos - (pos - area.pos);
    uint8_t* shadow_buf = ShadowAddrAt(shadow_buf_start_pos, config_);

    for (int dy = 0; dy < copy_area.size.y; ++dy) {
        SyscallCopyToFrameBuffer(shadow_buf, copy_area.pos.x,
                                 dy + copy_area.pos.y,
                                 bytes_per_pixel * copy_area.size.x);
        shadow_buf += bytes_per_pixel * config_.pixels_per_scan_line;
    }
    return MAKE_ERROR(Error::kSuccess);
}

void ShadowBuffer::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
    const auto bytes_per_pixel = BytesPerPixel();
    const auto bytes_per_scan_line = BytesPerScanLine(config_);

    if (dst_pos.y < src.pos.y) {
        uint8_t* dst_buf = ShadowAddrAt(dst_pos, config_);
        const uint8_t* src_buf = ShadowAddrAt(src.pos, config_);
        for (int y = 0; y < src.size.y; ++y) {
            memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
            dst_buf += bytes_per_scan_line;
            src_buf += bytes_per_scan_line;
        }
    } else {
        uint8_t* dst_buf =
            ShadowAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, config_);
        const uint8_t* src_buf =
            ShadowAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, config_);
        for (int y = 0; y < src.size.y; ++y) {
            memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
            dst_buf -= bytes_per_scan_line;
            src_buf -= bytes_per_scan_line;
        }
    }
}