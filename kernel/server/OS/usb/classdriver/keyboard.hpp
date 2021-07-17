#pragma once

#include <functional>
#include "server/OS/usb/classdriver/hid.hpp"

namespace usb {
  class HIDKeyboardDriver : public HIDBaseDriver {
   public:
    HIDKeyboardDriver(Device* dev, int interface_index);

    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

    Error OnDataReceived() override;

    using ObserverType = void (uint8_t modifier, uint8_t keycode, bool press);
    void SubscribeKeyPush(std::function<ObserverType> observer);
    static std::function<ObserverType> default_observer;

   private:
    std::array<std::function<ObserverType>, 4> observers_;
    int num_observers_ = 0;

    void NotifyKeyPush(uint8_t modifier, uint8_t keycode, bool press);
};
}