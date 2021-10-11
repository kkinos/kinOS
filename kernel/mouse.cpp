#include "mouse.hpp"

#include <limits>
#include <memory>

#include "graphics.hpp"
#include "task.hpp"
#include "usb/classdriver/mouse.hpp"

void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x,
                        int8_t displacement_y) {
    Message msg{Message::kMouseMove};
    msg.arg.mouse_move.dx = displacement_x;
    msg.arg.mouse_move.dy = displacement_y;
    msg.arg.mouse_move.buttons = buttons;
    uint64_t id = task_manager->FindTask("servers/mikanos");
    task_manager->SendMessage(id, msg);
}

void InitializeMouse() {
    auto mouse = std::make_shared<Mouse>();

    usb::HIDMouseDriver::default_observer =
        [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
            mouse->OnInterrupt(buttons, displacement_x, displacement_y);
        };
}