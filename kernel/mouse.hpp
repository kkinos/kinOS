#pragma once

#include <memory>

#include "graphics.hpp"

class Mouse {
   public:
    Mouse(){};
    void OnInterrupt(uint8_t buttons, int8_t displacement_x,
                     int8_t displacement_y);
};

void InitializeMouse();
