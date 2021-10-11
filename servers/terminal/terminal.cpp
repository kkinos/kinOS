#include "terminal.hpp"

#include <cstdio>

Vector2D<int> CalcCursorPos() {
    return kTopLeftMargin + Vector2D<int>{4 + 8 * cursorx, 4 + 16 * cursory};
}

void DrawCursor(uint64_t layer_id, bool visible) {
    const auto color = visible ? 0xffffff : 0;
    WinFillRectangle(layer_id, true, CalcCursorPos().x, CalcCursorPos().y, 7,
                     15, color);
}

Rectangle<int> BlinkCursor(uint64_t layer_id) {
    cursor_visible_ = !cursor_visible_;
    DrawCursor(layer_id, cursor_visible_);
    return {CalcCursorPos(), {7, 15}};
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
        buffer[cursory][cursorx] = c;
        if (cursorx == kColumns - 1) {
            newline();
        } else {
            ++cursorx;
        }
    }
}

void Print(uint64_t layer_id, const char *s, std::optional<size_t> len) {
    DrawCursor(layer_id, false);
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
    DrawCursor(layer_id, true);
}

void PrintInGreen(uint64_t layer_id, char c) {
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
        buffer[cursory][cursorx] = c;
        if (cursorx == kColumns - 1) {
            newline();
        } else {
            ++cursorx;
        }
    }
}

void PrintInGreen(uint64_t layer_id, const char *s, std::optional<size_t> len) {
    DrawCursor(layer_id, false);
    if (len) {
        for (size_t i = 0; i < *len; ++i) {
            PrintInGreen(layer_id, *s);
            ++s;
        }
    } else {
        while (*s) {
            PrintInGreen(layer_id, *s);
            ++s;
        }
    }
    DrawCursor(layer_id, true);
}

int PrintT(uint64_t layer_id, const char *format, ...) {
    va_list ap;
    int result;
    char s[128];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    Print(layer_id, s);
    return result;
}

Rectangle<int> InputKey(uint64_t layer_id, uint8_t modifier, uint8_t keycode,
                        char ascii) {
    DrawCursor(layer_id, false);
    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n') {
        linebuf_[linebuf_index_] = 0;
        linebuf_index_ = 0;
        cursorx = 0;

        if (cursory < kRows - 1) {
            ++cursory;
        } else {
            Scroll1(layer_id);
        }

        ExecuteLine(layer_id);
        PrintInGreen(layer_id, "user@KinOS:\n");
        PrintInGreen(layer_id, "$");
    } else if (ascii == '\b') {
        if (cursorx > 0) {
            --cursorx;
            buffer[cursory][cursorx] = '\0';
            WinFillRectangle(layer_id, true, CalcCursorPos().x,
                             CalcCursorPos().y, 8, 16, 0);
            draw_area.pos = CalcCursorPos();
            if (linebuf_index_ > 0) {
                --linebuf_index_;
            }
        }
    } else if (ascii != 0) {
        if (cursorx < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
            linebuf_[linebuf_index_] = ascii;
            ++linebuf_index_;
            buffer[cursory][cursorx] = ascii;
            WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y,
                         0xffffff, ascii);
            ++cursorx;
        }
    }
    DrawCursor(layer_id, true);
    return draw_area;
};

void ExecuteLine(uint64_t layer_id) {
    char *command = &linebuf_[0];

    if (strcmp(command, "help") == 0) {
        PrintT(layer_id, "help!\n");
    } else {
        ExecuteFile(layer_id);
    }
}

void ExecuteFile(uint64_t layer_id) {
    char *command = &linebuf_[0];
    char *first_arg = strchr(&linebuf_[0], ' ');
    if (first_arg) {
        *first_arg = 0;
        ++first_arg;
    }

    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        PrintT(layer_id, "cannot find application management server\n");
        return;
    }

    smsg[0].type = Message::kExecuteFile;
    smsg[0].arg.executefile.exist = false;
    smsg[0].arg.executefile.directory = false;

    int i = 0;
    while (*command) {
        if (i > 14) {
            PrintT(layer_id, "too long file name\n");
            return;
        }
        smsg[0].arg.executefile.filename[i] = *command;
        ++i;
        ++command;
    }
    smsg[0].arg.executefile.filename[i] = '\0';

    if (first_arg) {
        i = 0;
        while (*first_arg) {
            if (i > 30) {
                PrintT(layer_id, "too long argument\n");
                return;
            }
            smsg[0].arg.executefile.arg[i] = *first_arg;
            ++i;
            ++first_arg;
        }
        smsg[0].arg.executefile.arg[i] = '\0';
    }
    SyscallSendMessage(smsg, am_id);

    while (true) {
        SyscallClosedReceiveMessage(rmsg, 1, am_id);

        switch (rmsg[0].type) {
            case Message::kError:
                if (rmsg[0].arg.error.retry) {
                    SyscallSendMessage(smsg, am_id);
                    break;
                } else {
                    PrintT(layer_id, "error at other server\n");
                    return;
                }

            case Message::kExecuteFile:
                if (!rmsg[0].arg.executefile.exist) {
                    PrintT(layer_id, "no such file\n");
                    return;
                }
                if (rmsg[0].arg.executefile.directory) {
                    PrintT(layer_id, "this is directory\n");
                    return;
                }
            default:
                PrintT(layer_id, "Unknown message type: %d\n", rmsg[0].type);
                break;
        }
    }
}

extern "C" void main() {
    int layer_id = OpenWindow(kColumns * 8 + 12 + Marginx,
                              kRows * 16 + 12 + Marginy, 20, 20);
    if (layer_id == -1) {
        exit(1);
    }

    WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth,
                     kCanvasHeight, 0);
    PrintInGreen(layer_id, "user@KinOS:\n");
    PrintInGreen(layer_id, "$");

    SyscallWriteKernelLog("[ terminal ] ready\n");

    while (true) {
        SyscallOpenReceiveMessage(rmsg, 1);

        if (rmsg[0].type == Message::kKeyPush) {
            if (rmsg[0].arg.keyboard.press) {
                const auto area = InputKey(
                    layer_id, rmsg[0].arg.keyboard.modifier,
                    rmsg[0].arg.keyboard.keycode, rmsg[0].arg.keyboard.ascii);
            }
        }
    }
}