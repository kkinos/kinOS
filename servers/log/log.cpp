#include "log.hpp"

#include <cstdio>

Vector2D<int> CalcCursorPos() {
    return kTopLeftMargin + Vector2D<int>{4 + 8 * cursorx, 4 + 16 * cursory};
}

void Scroll1(uint64_t layer_id) {
    WinMoveRec(layer_id, false, Marginx + 4, Marginy + 4, Marginx + 4,
               Marginy + 4 + 16, 8 * kColumns, 16 * (kRows - 1));
    WinFillRectangle(layer_id, false, 4, 24 + 4 + 16 * cursory, (8 * kColumns),
                     16, 0);
    WinRedraw(layer_id);
}

void Print(uint64_t layer_id, char c) {
    auto newline = [layer_id]() {
        cursorx = 0;
        if (cursory < kRows - 1) {
            ++cursory;
        } else {
            Scroll1(layer_id);
        }
    };

    if (c == '\n') {
        newline();
    } else {
        WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y,
                     0xffffff, c);
        if (cursorx == kColumns - 1) {
            newline();
        } else {
            ++cursorx;
        }
    }
}

void Print(uint64_t layer_id, const char *s, std::optional<size_t> len) {
    if (len) {
        for (size_t i = 0; i < *len; ++i) {
            Print(layer_id, *s);
            ++s;
        }
    } else {
        while (*s) {
            Print(layer_id, *s);
            ++s;
        }
    }
}

void PrintUserName(uint64_t layer_id, char c) {
    auto newline = [layer_id]() {
        cursorx = 0;
        if (cursory < kRows - 1) {
            ++cursory;
        } else {
            Scroll1(layer_id);
        }
    };

    if (c == '\n') {
        newline();
    } else {
        WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y,
                     0x29ff86, c);

        if (cursorx == kColumns - 1) {
            newline();
        } else {
            ++cursorx;
        }
    }
}

void PrintUserName(uint64_t layer_id, const char *s,
                   std::optional<size_t> len) {
    if (len) {
        for (size_t i = 0; i < *len; ++i) {
            PrintUserName(layer_id, *s);
            ++s;
        }
    } else {
        while (*s) {
            PrintUserName(layer_id, *s);
            ++s;
        }
    }
}

int PrintToTerminal(uint64_t layer_id, const char *format, ...) {
    va_list ap;
    int result;
    char s[128];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    Print(layer_id, s);
    return result;
}

extern "C" void main() {
    int layer_id = OpenWindow(kColumns * 8 + 12 + Marginx,
                              kRows * 16 + 12 + Marginy, 20, 20);
    if (layer_id == -1) {
        exit(1);
    }

    WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth,
                     kCanvasHeight, 0);

    while (true) {
        char buf[128] = {};
        auto [n, err] = SyscallReadKernelLog(&buf[0], sizeof(buf));
        if (err) {
            continue;
        }
        PrintToTerminal(layer_id, buf);
    }
}