#include <cstdio>


#include "console.hpp"
#include "font.hpp"
#include "layer.hpp"


Console::Console(const PixelColor& fg_color, const PixelColor& bg_color, const PixelColor& tn_color)
    : fg_color_{fg_color}, bg_color_{bg_color}, tn_color_{tn_color},
      cursor_row_{2}, cursor_column_{2}, layer_id_{0} {}

/**
 * @brief コンソールに文字を表示する
 * 
 * @param s 文字列
 */
void Console::PutString(const char* s) {
  while (*s) {
    if (*s == '\n') {
      Newline();
    } else if (cursor_column_ < kColumns - 1) {
       WriteAscii(*writer_, Vector2D<int>{8 * cursor_column_, 16 * cursor_row_}, *s, fg_color_);
      ++cursor_column_;
    }
    ++s;
  }
  if (layer_manager) {
    layer_manager->Draw(layer_id_);
  }
}

void Console::Newline() {
    cursor_column_ = 2;
    if (cursor_row_ < kRows - 1) {
        ++cursor_row_;
        return;
    } 
    Rectangle<int> move_src{{0, 16 * (kTitleRows + 1)}, {8 * kColumns, 16 * (kRows - kTitleRows- 1)}};
    window_->Move({0, 16 * kTitleRows}, move_src);
    FillRectangle(*writer_, {0, 16 * (kRows - 1)}, {8 * kColumns, 16}, bg_color_);
    
}

void Console::SetWriter(PixelWriter* writer) {
  if (writer == writer_) {
    return;
  }
  writer_ = writer;
  /*window_.reset();*/
  Refresh();
}

void Console::SetWindow(const std::shared_ptr<Window>& window) {
  if (window == window_) {
    return;
  }
  window_ = window;
  writer_ = window->Writer();
  Refresh();
}

void Console::SetLayerID(unsigned int layer_id) {
  layer_id_ = layer_id;
}

unsigned int Console::LayerID() const {
  return layer_id_;
}

void Console::Clear() {
  cursor_column_ = 2;
  cursor_row_ = kTitleRows;
  FillRectangle(*writer_, {0, 16 * kTitleRows}, {8 * kColumns, 16 * (kRows - kTitleRows- 1)}, bg_color_);
  if (layer_manager) {
    layer_manager->Draw(layer_id_);
  }
}

void Console::Refresh() {
    FillRectangle(*writer_, {0, 0}, {8 * kColumns, 16 * kRows}, bg_color_);
}

namespace {
  char console_buf[sizeof(Console)];
}

Console* console;

void InitializeConsole() {
  console = new(console_buf) Console{
    kDesktopFGColor, kDesktopBGColor, kDesktopTNColor,
  };
  console->SetWriter(screen_writer);

}

int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}