#pragma once

enum class LayerOperation {
  Move, MoveRelative, Draw, DrawArea
};

struct Message {
  enum Type {
    kInterruptXHCI,
    kTimerTimeout,
    kKeyPush,
    kLayer,
    kLayerFinish,
    kMouseMove,
    kMouseButton,
    kWindowActive,
    kPipe,
    kCreateAppTask,
    aMouseMove,
    aKeyPush,
    aOpenWindow,
    aLayerId,
    aWinFillRectangle,
    aWinWriteChar,
    aWinRedraw,
  } type;

  uint64_t src_task;
  

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;

    struct {
      uint8_t modifier;
      uint8_t keycode;
      char ascii;
      int press;
    } keyboard;

    struct {
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;
    } layer;

    struct {
      int x, y;
      int dx, dy;
      uint8_t buttons;
    } mouse_move;

    struct {
      int x, y;
      int press; // 1: press, 0: release
      int button;
    } mouse_button;

    struct {
      int activate; // 1: activate, 0: deactivate
    } window_active;

    struct {
      char data[16];
      uint8_t len;
    } pipe;
    
    struct {
      int pid, cid;
    } create;

    struct {
      int w, h, x, y;
    } openwindow;

    struct {
      int layerid;
    } layerid;

    struct {
      int layer_id, x, y, w, h; 
      bool draw;
      uint32_t color;
    } winfillrectangle;

    struct {
      int layer_id, x, y;
      uint32_t color;
      char c;
    } winwritechar;

  } arg;
};