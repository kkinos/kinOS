#include "shell.hpp"

Vector2D<int> CalcCursorPos() {
    return kTopLeftMargin +
        Vector2D<int>{4 + 8 * cursorx, 4 + 16 * cursory};
}

void DrawCursor(uint64_t layer_id, bool visible) {
    const auto color = visible ? 0xffffff : 0;
    SyscallWinFillRectangle(layer_id, CalcCursorPos().x, CalcCursorPos().y, 7, 15, color);

}

Rectangle<int> BlinkCursor(uint64_t layer_id) {
    cursor_visible_ = !cursor_visible_;
    DrawCursor(layer_id, cursor_visible_);

    return {CalcCursorPos(), {7, 15}};
}

void Scroll1(uint64_t layer_id) {
    SyscallWinFillRectangle(layer_id, Marginx, Marginy, kCanvasWidth , kCanvasHeight , 0);
    for (int row = 0; row < kRows - 1; ++row) {
        memcpy(buffer[row], buffer[row + 1], kColumns + 1);
        SyscallWinWriteString(layer_id, 4 + 4, 4 + 24 + 16 * row, 0xffffff, buffer[row]);
    }
    memset(buffer[kRows - 1], 0, kColumns + 1);
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
      char C[] = {c, '\0'}; 
      SyscallWinWriteString(layer_id, CalcCursorPos().x, CalcCursorPos().y, 0xffffff, C);
      buffer[cursory][cursorx] = c;
      if (cursorx == kColumns - 1) {
        newline();
      } else {
          ++cursorx;
    }
  }
}

void Print(uint64_t layer_id, const char* s, std::optional<size_t> len) {
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

Rectangle<int> InputKey(
    uint64_t layer_id, uint8_t modifier, uint8_t keycode, char ascii) {
        DrawCursor(layer_id, false);

        Rectangle<int> draw_area{CalcCursorPos(), {8*2, 16}};


         if (ascii == '\n') {
            linebuf_[linebuf_index_] = 0;
            linebuf_index_ = 0;
            
            cursorx = 0;
            if(cursory < kRows - 1) {
                ++cursory;
            } else {
                Scroll1(layer_id);
            }

            ExecuteLine(layer_id);
            Print(layer_id, "$");

        } else if (ascii == '\b') {
            if (cursorx > 0) {
                --cursorx;
                buffer[cursory][cursorx] = '\0';
                SyscallWinFillRectangle(layer_id, CalcCursorPos().x,CalcCursorPos().y, 8, 16, 0);
                draw_area.pos = CalcCursorPos();
                if (linebuf_index_ > 0) {
                    --linebuf_index_;
                }
            }

        } else if (ascii != 0) {
            if (cursorx < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
                linebuf_[linebuf_index_] = ascii;
                ++linebuf_index_;
                char c[] = {ascii,'\0'};
                buffer[cursory][cursorx] = ascii;
                SyscallWinWriteString(layer_id, CalcCursorPos().x, CalcCursorPos().y, 0xffffff, c);
                ++cursorx;
            }
        }
        DrawCursor(layer_id, true);
        
        return draw_area;
    };

void ExecuteLine(uint64_t layer_id) {
    char* command = &linebuf_[0];
    kexec2(command);
}






extern "C" void main() {
    auto [layer_id, err_openwin]
        = SyscallOpenWindow(kColumns * 8 + 12 + Marginx, kRows * 16 + 12 + Marginy, 20, 20, "shell");
    if (err_openwin) {
        exit(err_openwin);
        }

    SyscallWinFillRectangle(layer_id, Marginx, Marginy, kCanvasWidth , kCanvasHeight , 0);
    Print(layer_id, "$");

    AppEvent events[1];

    while (true) {
        auto [ n, err ] = SyscallReadEvent(events, 1);
        if (err) {
            printf("ReadEvent failed: %s\n", strerror(err));
            break;
        }
        if (events[0].type == AppEvent::kKeyPush) {
            if (events[0].arg.keypush.press) {
        
            const auto area = InputKey( layer_id,
                                        events[0].arg.keypush.modifier,
                                        events[0].arg.keypush.keycode,
                                        events[0].arg.keypush.ascii);
            }
        }

  }

    


}