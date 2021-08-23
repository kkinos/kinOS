#include "shadow_buffer.hpp"
#include "font.hpp"

namespace {
    /**
     * @brief 一つのpixelは4bytes
     * 
     * @return int bytes
     */
    int BytesPerPixel() { return 4; }

    /**
     * @brief posからshadow_bufferにおけるアドレスを計算
     * 
     * @param pos 
     * @param config 
     * @return uint8_t* 
     */
    uint8_t* ShadowAddrAt(Vector2D<int> pos, const ShadowBufferConfig& config) {
        return config.shadow_buffer + BytesPerPixel() * (config.pixels_per_scan_line * pos.y + pos.x);
    }

    /**
     * @brief shadowbufferの横一列が何bytesか計算
     * 
     * @param config 
     * @return int 
     */
    int BytesPerScanLine(const ShadowBufferConfig& config) {
    return BytesPerPixel() * config.pixels_per_scan_line;
  }


  Vector2D<int> ShadowBufferSize(const ShadowBufferConfig& config) {
    return {static_cast<int>(config.horizontal_resolution),
            static_cast<int>(config.vertical_resolution)};
  }
}

Error ShadowBuffer::Initialize(const ShadowBufferConfig& config) {
    config_ = config;
    
    const auto bytes_per_pixel = 4; /*1ピクセル4bytes*/
    buffer_.resize(bytes_per_pixel * config_.horizontal_resolution * config_.vertical_resolution); /*表示領域と同じ大きさのバッファを確保*/
    config_.shadow_buffer = buffer_.data();
    config_.pixels_per_scan_line = config_.horizontal_resolution;
    writer_ = std::make_unique<ShadowBufferWriter>(config_);
    return MAKE_ERROR(Error::kSuccess);
    

}

/**
 * @brief FrameBuffer(表示領域)にShadowBufferをコピー
 * 
 * @param pos 
 * @return Error 
 */

Error ShadowBuffer::CopyToFrameBuffer(Vector2D<int> pos) {
    const auto bytes_per_pixel = 4; /*1ピクセル4bytes*/
    const auto shadow_buffer_width = config_.horizontal_resolution;
    const auto shadow_buffer_height = config_.vertical_resolution;
    const auto frame_buffer_width = static_cast<uint32_t>(ScreenSize().x);
    const auto frame_buffer_height = static_cast<uint32_t>(ScreenSize().y);
    const int copy_start_x = std::max(pos.x, 0);
    const int copy_start_y = std::max(pos.y, 0);
    const int copy_end_x = std::min(pos.x + shadow_buffer_width, frame_buffer_width);
    const int copy_end_y = std::min(pos.y + shadow_buffer_height, frame_buffer_height);
    
    const auto bytes_per_copy_line = bytes_per_pixel * (copy_end_x - copy_start_x);

    const uint8_t* shadow_buf = config_.shadow_buffer;
    for (int dy = 0; dy < copy_end_y - copy_start_y; ++dy) {
        SyscallCopyToFrameBuffer(shadow_buf, copy_start_x, dy + copy_start_y, bytes_per_copy_line);
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
        uint8_t* dst_buf = ShadowAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, config_);
        const uint8_t* src_buf = ShadowAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, config_);
        for (int y = 0; y < src.size.y; ++y) {
        memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
        dst_buf -= bytes_per_scan_line;
        src_buf -= bytes_per_scan_line;
    }
    }
}