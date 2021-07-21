#include <cstring>
#include <cstdlib>
#include <cstdint>
#include "../../kernel/logger.hpp"

extern "C" int64_t SyscallLogString(LogLevel, const char*);

extern "C" int main() {
    SyscallLogString(kWarn, "hello from server \n");
    exit(0);
}