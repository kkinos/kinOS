#include "frame_buffer.hpp"

Error FrameBuffer::Initialize(const FrameBufferConfig& config) {
    config_ = config;
    return MAKE_ERROR(Error::kSuccess);
}
