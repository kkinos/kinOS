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
        buffer[cursory][cursorx] = c;
        if (cursorx == kColumns - 1) {
            newline();
        } else {
            ++cursorx;
        }
    }
}

void PrintUserName(uint64_t layer_id, const char *s,
                   std::optional<size_t> len) {
    DrawCursor(layer_id, false);
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
    DrawCursor(layer_id, true);
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
        PrintUserName(layer_id, "user@KinOS:\n");
        PrintUserName(layer_id, "$");
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
        PrintToTerminal(layer_id, "help!\n");
    } else {
        ExecuteFile(layer_id);
    }
    // kexec(command);
}

/**
 * @brief ファイルを実行する
 *
 * @param layer_id
 */
void ExecuteFile(uint64_t layer_id) {
    char *command = &linebuf_[0];

    // amサーバを探す
    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        PrintToTerminal(layer_id,
                        "cannot find application management server\n");
        return;
    }

    smsg[0].type = Message::aExecuteFile;
    smsg[0].arg.executefile.exist = false;
    smsg[0].arg.executefile.directory = false;

    int i = 0;
    while (*command) {
        if (i > 14) {
            PrintToTerminal(layer_id, "too large file name\n");
            return;
        }
        smsg[0].arg.executefile.filename[i] = *command;
        ++i;
        ++command;
    }
    smsg[0].arg.executefile.filename[i] = '\0';
    SyscallSendMessage(smsg, am_id);

    while (true) {
        SyscallCloseReceiveMessage(rmsg, 1, am_id);

        if (rmsg[0].type == Message::Error) {
            if (rmsg[0].arg.error.retry) {
                SyscallSendMessage(smsg, am_id);
            } else {
                PrintToTerminal(layer_id, "Error at other server\n");
                return;
            }
        }
        if (rmsg[0].type == Message::aExecuteFile) {
            // 指定されたファイルが存在しない
            if (!rmsg[0].arg.executefile.exist) {
                PrintToTerminal(layer_id, "no such file\n");
                return;
            }
            // 指定されたファイルがディレクトリ
            if (rmsg[0].arg.executefile.directory) {
                PrintToTerminal(layer_id, "this is directory\n");
                return;
            }
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
    PrintUserName(layer_id, "user@KinOS:\n");
    PrintUserName(layer_id, "$");

    SyscallWriteKernelLog("terminal: Start\n");

    while (true) {
        SyscallOpenReceiveMessage(rmsg, 1);

        if (rmsg[0].type == Message::aKeyPush) {
            if (rmsg[0].arg.keyboard.press) {
                const auto area = InputKey(
                    layer_id, rmsg[0].arg.keyboard.modifier,
                    rmsg[0].arg.keyboard.keycode, rmsg[0].arg.keyboard.ascii);
            }
        }
    }
}