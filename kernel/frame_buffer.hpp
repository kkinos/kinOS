#pragma once

#include <vector>
#include <memory>

#include "frame_buffer_config.hpp"
#include "error.hpp"

class FrameBuffer {
 public:
  Error Initialize(const FrameBufferConfig& config);
  const FrameBufferConfig& Config() const { return config_; }

 private:
  FrameBufferConfig config_{};

};
