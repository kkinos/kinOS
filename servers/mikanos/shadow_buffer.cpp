#include "shadow_buffer.hpp"
#include "font.hpp"

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