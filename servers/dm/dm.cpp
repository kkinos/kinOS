#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../libs/kinos/common/syscall.h"
#include "pci.hpp"

extern "C" void main() {
    auto err = pci::ScanAllBus();
    printf("ScanBus :%s\n", err.Name());

    printf("num of device : %d\n", pci::num_device);
    for (int i = 0; i < pci::num_device; ++i) {
        const auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        printf("%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
               dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
               dev.class_code.base, dev.class_code.sub,
               dev.class_code.interface);
    };
    exit(0);
}