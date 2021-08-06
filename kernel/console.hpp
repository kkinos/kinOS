#pragma once 

#include <memory>
#include "graphics.hpp"
#include "window.hpp"

class Console {
 public:
  static const int kRows = 34, kColumns = 85;
  static const int kTitleRows = 14;

  Console(const PixelColor& fg_color, const PixelColor& bg_color, const PixelColor& tn_color);
  void PutString(const char* s);
  void PutTaskName(const char* task_name);
  void SetWriter(PixelWriter* writer);
  void SetWindow(const std::shared_ptr<Window>& window);
  void SetLayerID(unsigned int layer_id);
  unsigned int LayerID() const;


 private:
  void Newline();
  void Refresh();

  PixelWriter* writer_;
  std::shared_ptr<Window> window_;
  const PixelColor fg_color_, bg_color_, tn_color_;
  int cursor_row_, cursor_column_;
  unsigned int layer_id_;

};

extern Console* console;
extern int taskid;

void InitializeConsole();

int printk(const char* format, ...);
int printt(const char* format, ...);