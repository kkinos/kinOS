#pragma once

#include <stdint.h>

struct ShadowBufferConfig {
  uint8_t* shadow_buffer;
  uint32_t pixels_per_scan_line;
  uint32_t horizontal_resolution;
  uint32_t vertical_resolution;
};