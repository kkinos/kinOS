#include "terminal.hpp"

#include <ctype.h>
#include <errno.h>

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

Rectangle<int> HistoryUpDown(int direction, uint64_t layer_id) {
    if (direction == -1 && cmd_history_index_ >= 0) {
        --cmd_history_index_;
    } else if (direction == 1 && cmd_history_index_ + 1 < cmd_history_.size()) {
        ++cmd_history_index_;
    }

    cursorx = 1;
    const auto first_pos = CalcCursorPos();
    Rectangle<int> draw_area{first_pos, {8 * (kColumns - 1), 16}};
    WinFillRectangle(layer_id, true, draw_area.pos.x, draw_area.pos.y,
                     draw_area.size.x, draw_area.size.y, 0);

    char *history = "";
    if (cmd_history_index_ >= 0) {
        history = &cmd_history_[cmd_history_index_][0];
    }
    strcpy(&linebuf_[0], history);
    linebuf_index_ = strlen(history);
    WinWriteString(layer_id, true, first_pos.x, first_pos.y, 0xffffff, history);
    cursorx = linebuf_index_ + 1;
    return draw_area;
}

Rectangle<int> InputKey(uint64_t layer_id, uint8_t modifier, uint8_t keycode,
                        char ascii) {
    DrawCursor(layer_id, false);
    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n') {
        linebuf_[linebuf_index_] = 0;
        if (linebuf_index_ > 0) {
            cmd_history_.pop_back();
            cmd_history_.push_front(linebuf_);
        }
        linebuf_index_ = 0;
        cmd_history_index_ = -1;
        cursorx = 0;

        if (cursory < kRows - 1) {
            ++cursory;
        } else {
            Scroll1(layer_id);
        }

        ExecuteLine(layer_id);
        PrintInGreen(layer_id, "kinOS:>\n");
        PrintInGreen(layer_id, "$");
    } else if (ascii == '\b') {
        if (cursorx > 0) {
            --cursorx;

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

            WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y,
                         0xffffff, ascii);
            ++cursorx;
        }
    } else if (keycode == 0x51) {  // down arrow
        draw_area = HistoryUpDown(-1, layer_id);
    } else if (keycode == 0x52) {  // up arrow
        draw_area = HistoryUpDown(1, layer_id);
    }
    DrawCursor(layer_id, true);
    return draw_area;
};

void ExecuteLine(uint64_t layer_id) {
    char *command = &linebuf_[0];

    if (strcmp(command, "help") == 0) {
        Print(layer_id, "command [ls apps] show applications\n");
    } else if (strcmp(command, "clear") == 0) {
        WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth,
                         kCanvasHeight, 0);
        cursory = 0;
    } else {
        ExecuteFile(layer_id);
    }
}

void ExecuteFile(uint64_t layer_id) {
    char *command = &linebuf_[0];
    char *first_arg = strchr(&linebuf_[0], ' ');
    char *redir_char = strchr(&linebuf_[0], '>');
    if (first_arg) {
        *first_arg = 0;
        ++first_arg;
    }
    if (redir_char) {
        *redir_char = 0;
        ++redir_char;
    }

    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        PrintT(layer_id, "cannot find application management server\n");
        return;
    }

    send_message[0].type = Message::kExecuteFile;
    memset(send_message[0].arg.executefile.filename, 0,
           sizeof(send_message[0].arg.executefile.filename));
    int i = 0;
    while (*command) {
        if (i > 25) {
            PrintT(layer_id, "too long file name\n");
            return;
        }

        send_message[0].arg.executefile.filename[i] = *command;
        ++i;
        ++command;
    }
    send_message[0].arg.executefile.filename[i] = '\0';

    memset(send_message[0].arg.executefile.arg, 0,
           sizeof(send_message[0].arg.executefile.arg));
    i = 0;
    if (first_arg) {
        while (*first_arg) {
            if (i > 25) {
                PrintT(layer_id, "too long argument\n");
                return;
            }

            if (first_arg[0] == '$' && first_arg[1] == '?') {
                char exit_code[16];
                char *p;
                sprintf(exit_code, "%d", last_exit_code_);
                p = &exit_code[0];
                while (*p) {
                    send_message[0].arg.executefile.arg[i] = *p;
                    ++p;
                    ++i;
                }
                ++first_arg;
                ++first_arg;

            } else {
                send_message[0].arg.executefile.arg[i] = *first_arg;
                ++i;
                ++first_arg;
            }
        }
    }

    send_message[0].arg.executefile.arg[i] = '\0';

    if (redir_char) {
        send_message[0].arg.executefile.redirect = true;
    } else {
        send_message[0].arg.executefile.redirect = false;
    }

    SyscallSendMessage(send_message, am_id);

    if (redir_char) {
        char *redir_filename = &redir_char[0];
        while (isspace(*redir_filename)) {
            ++redir_filename;
        }

        send_message[0].type = Message::kRedirect;
        memset(send_message[0].arg.redirect.filename, 0,
               sizeof(send_message[0].arg.redirect.filename));
        i = 0;
        if (redir_filename) {
            while (*redir_filename) {
                if (i > 25) {
                    PrintT(layer_id, "too long file name\n");
                    return;
                }
                send_message[0].arg.redirect.filename[i] = *redir_filename;
                ++i;
                ++redir_filename;
            }
        }
        SyscallSendMessage(send_message, am_id);
    }

    while (true) {
        SyscallClosedReceiveMessage(received_message, 1, am_id);

        switch (received_message[0].type) {
            case Message::kError:
                if (received_message[0].arg.error.retry) {
                    SyscallSendMessage(send_message, am_id);
                    break;
                } else {
                    if (received_message[0].arg.error.err == ENOENT) {
                        last_exit_code_ = 1;
                        PrintT(layer_id, "no such file\n");
                        return;
                    } else if (received_message[0].arg.error.err == EISDIR) {
                        last_exit_code_ = 1;
                        PrintT(layer_id, "this is a directory\n");
                        return;
                    } else {
                        last_exit_code_ = 1;
                        PrintT(layer_id, "error at other server\n");
                        return;
                    }
                }
            case Message::kWrite:
                Print(layer_id, received_message[0].arg.write.data,
                      received_message[0].arg.write.len);
                break;

            case Message::kRead: {
                while (1) {
                    SyscallOpenReceiveMessage(received_message, 1);
                    if (received_message[0].type == Message::kKeyPush &&
                        received_message[0].arg.keyboard.press) {
                        if (received_message[0].arg.keyboard.modifier &
                            (kLControlBitMask | kRControlBitMask)) {
                            char s[3] = "^ ";
                            s[1] =
                                toupper(received_message[0].arg.keyboard.ascii);
                            if (received_message[0].arg.keyboard.keycode ==
                                7 /* D */) {
                                send_message[0].type = Message::kRead;
                                send_message[0].arg.read.len = 0;  // EOT
                                PrintT(layer_id, "%s\n", s);
                                SyscallSendMessage(send_message, am_id);
                                break;
                            }
                        } else {
                            PrintT(layer_id,
                                   &received_message[0].arg.keyboard.ascii);
                            send_message[0].type = Message::kRead;
                            send_message[0].arg.read.data[0] =
                                received_message[0].arg.keyboard.ascii;
                            send_message[0].arg.read.len = 1;
                            SyscallSendMessage(send_message, am_id);
                            break;
                        }
                    }
                }

            } break;

            case Message::kExitApp:
                last_exit_code_ = received_message[0].arg.exitapp.result;
                return;

            default:
                PrintT(layer_id, "Unknown message type: %d\n",
                       received_message[0].type);
                break;
        }
    }
}

extern "C" void main() {
    int layer_id = OpenWindow(kColumns * 8 + 12 + Marginx,
                              kRows * 16 + 12 + Marginy, 20, 20, "terminal");
    if (layer_id == -1) {
        exit(1);
    }

    WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth,
                     kCanvasHeight, 0);
    PrintInGreen(layer_id, "kinOS:>\n");
    PrintInGreen(layer_id, "$");

    cmd_history_.resize(8);

    SyscallWriteKernelLog("[ terminal ] ready\n");

    while (true) {
        SyscallOpenReceiveMessage(received_message, 1);

        if (received_message[0].type == Message::kKeyPush) {
            if (received_message[0].arg.keyboard.press) {
                const auto area = InputKey(
                    layer_id, received_message[0].arg.keyboard.modifier,
                    received_message[0].arg.keyboard.keycode,
                    received_message[0].arg.keyboard.ascii);
            }
        }
    }
}