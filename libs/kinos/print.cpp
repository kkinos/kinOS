#include "print.hpp"

#include <stdarg.h>
#include <stdio.h>

#include "syscall.h"

int Print(const char *format, ...) {
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    SyscallWriteKernelLog(s);
    return result;
}