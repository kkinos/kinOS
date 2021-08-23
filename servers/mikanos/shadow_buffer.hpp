#pragma once

#include <vector>
#include <memory>

#include "graphics.hpp"
#include "../../libs/common/error.hpp"

class ShadowBuffer {
 public:
  Error Initialize(const ShadowBufferConfig& config);
  Error CopyToFrameBuffer(Vector2D<int> pos);
  void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

  ShadowBufferWriter& Writer() { return *writer_; }

 private:
  ShadowBufferConfig config_{};
  std::vector<uint8_t> buffer_{};
  std::unique_ptr<ShadowBufferWriter> writer_{};
};