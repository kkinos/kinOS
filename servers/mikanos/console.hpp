/**
 * @file console.hpp
 *
 * コンソール描画のプログラムを集めたファイル
 */

#pragma once 

#include <memory>
#include "graphics.hpp"
#include "font.hpp"

/**
 * @brief コンソールクラス
 * 
 */
class Console {
 public:
  static const int kRows = 34, kColumns = 85;
  static const int kTitleRows = 14;

  Console(const PixelColor& fg_color, const PixelColor& bg_color, const PixelColor& tn_color);
  void PutString(const char* s);
  /**
  void PutTaskName(const char* task_name);
  void SetWriter(PixelWriter* writer);
  void SetWindow(const std::shared_ptr<Window>& window);
  void SetLayerID(unsigned int layer_id);
  unsigned int LayerID() const;
  void Clear();
  */


 private:
  void Newline();
  void Refresh();

  const PixelColor fg_color_, bg_color_, tn_color_;
  int cursor_row_, cursor_column_;
  unsigned int layer_id_;

};

extern Console* console;

/**
extern int taskid;
*/

void InitializeConsole();

/**
int printk(const char* format, ...);
int printt(const char* format, ...);
*/

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
       WriteAscii( Vector2D<int>{8 * cursor_column_, 16 * cursor_row_}, *s, fg_color_);
      ++cursor_column_;
    }
    ++s;
  }
  /**
  if (layer_manager) {
    layer_manager->Draw(layer_id_);
  }
  */
}

void Console::Newline() {
    cursor_column_ = 2;
    if (cursor_row_ < kRows - 1) {
        ++cursor_row_;
        return;
    } 
    Rectangle<int> move_src{{0, 16 * (kTitleRows + 1)}, {8 * kColumns, 16 * (kRows - kTitleRows- 1)}};
    /*window_->Move({0, 16 * kTitleRows}, move_src);*/
    FillRectangle( {0, 16 * (kRows - 1)}, {8 * kColumns, 16}, bg_color_);
    
}

namespace {
  char console_buf[sizeof(Console)];
}

Console* console;

void InitializeConsole() {
  console = new(console_buf) Console{
    kDesktopFGColor, kDesktopBGColor, kDesktopTNColor,
  };

}