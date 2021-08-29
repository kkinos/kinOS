#include "mouse.hpp"

#include <limits>
#include <memory>
#include "graphics.hpp"
#include "usb/classdriver/mouse.hpp"
#include "task.hpp"


void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  Message msg{Message::aMouseMove};
  msg.arg.mouse_move.dx = displacement_x;
  msg.arg.mouse_move.dy = displacement_y;
  msg.arg.mouse_move.buttons = buttons;
  task_manager->SendMessageToOs(msg);

}

void InitializeMouse() {
  auto mouse = std::make_shared<Mouse>();
 
  usb::HIDMouseDriver::default_observer =
    [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
      mouse->OnInterrupt(buttons, displacement_x, displacement_y);
    };


}