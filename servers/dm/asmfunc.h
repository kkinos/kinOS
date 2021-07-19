#pragma once

#include <stdint.h>
/*多分アセンブリをCから呼び出せるようにする*/
extern "C" {
  void IoOut32(uint16_t addr, uint32_t data);
  uint32_t IoIn32(uint16_t addr);
}