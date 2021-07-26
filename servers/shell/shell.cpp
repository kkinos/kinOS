#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>

#include "../syscall.h"

const int kRows = 15;
const int kColumns = 60;
const int Marginx = 4;
const int Marginy = 24;
const int kCanvasWidth = kColumns * 8 + 8;
const int kCanvasHeight = kRows * 16 + 8;
int cursorx = 0;
int cursory = 0;
bool cursor_visible_ = true;
const int kLineMax = 128;
int linebuf_index_{0};
std::array<char, kLineMax> linebuf_{};

void ExecuteLine();

template <typename T>
struct Vector2D {
  T x, y;
  template <typename U>
  Vector2D<T>& operator +=(const Vector2D<U>& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  template <typename U>
  Vector2D<T> operator +(const Vector2D<U>& rhs) const {
    auto tmp = *this;
    tmp += rhs;
    return tmp;
  }
};

template <typename T>
struct Rectangle {
  Vector2D<T> pos, size;
};

Vector2D<int> kTopLeftMargin = {4, 24};

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
            }
            ExecuteLine();
        } else if (ascii == '\b') {
            if (cursorx > 0) {
                --cursorx;
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
                char ascii_[] = {ascii,'\0'};
                SyscallWinWriteString(layer_id, CalcCursorPos().x, CalcCursorPos().y, 0xffffff, ascii_);
                ++cursorx;
            }
        }
        DrawCursor(layer_id, true);
        
        return draw_area;
    };

void ExecuteLine() {
    char* command = &linebuf_[0];
    char* first_arg = strchr(&linebuf_[0], ' ');

     if (first_arg) {
        *first_arg = 0;
        ++first_arg;
    }


    if (strcmp(command, "echo") == 0) {
        printf("%s", first_arg);
        printf("\n");
    } else {
        printf("no such command");
        printf("\n");
    }

}






extern "C" void main() {
    auto [layer_id, err_openwin]
        = SyscallOpenWindow(kColumns * 8 + 12 + Marginx, kRows * 16 + 12 + Marginy, 20, 20, "shell");
    if (err_openwin) {
        exit(err_openwin);
        }

    SyscallWinFillRectangle(layer_id, Marginx, Marginy, kCanvasWidth , kCanvasHeight , 0);

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