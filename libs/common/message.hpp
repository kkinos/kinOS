#pragma once

#include <stdint.h>

struct Message {
    enum Type {
        /*--------------------------------------------------------------------------
         * common message types
         *--------------------------------------------------------------------------
         */
        kError,
        kReady,
        /*--------------------------------------------------------------------------
         * message types to use kernel functions
         *--------------------------------------------------------------------------
         */
        kInterruptXHCI,
        kTimerTimeout,
        kLayer,
        kLayerFinish,
        kWindowActive,
        kPipe,
        kExpandTaskBuffer,
        kExcute,
        kExit,
        /*--------------------------------------------------------------------------
         * message types for servers and applications
         *--------------------------------------------------------------------------
         */
        kMouseMove,
        kKeyPush,
        kOpenWindow,
        kLayerId,
        kWinFillRectangle,
        kWinWriteChar,
        kWinRedraw,
        kQuit,
        kCloseWindow,
        kMouseButton,
        kWinDrawLine,
        kWinMoveRec,
        kWrite,
        kOpen,
        kCreateTask,
        kExecuteFile,
    } type;

    uint64_t src_task;

    union {
        struct {
            int retry;  // 1: true, 0: false
        } error;

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
            int x, y;
            int dx, dy;
            uint8_t buttons;
        } mouse_move;

        struct {
            int x, y;
            int press;  // 1: press, 0: release
            int button;
        } mouse_button;

        struct {
            int activate;  // 1: activate, 0: deactivate
        } window_active;

        struct {
            char data[16];
            uint8_t len;
        } pipe;

        struct {
            int pid, cid;
        } create;

        struct {
            uint64_t id;
            uint64_t p_id;
            int success;  // 1: success, 0: fail
        } execute;

        struct {
            uint64_t id;
            uint32_t bytes;
        } expand;

        struct {
            uint64_t id;
            uint64_t result;
        } exit;

        struct {
            int fd;
            char data[16];
            uint8_t len;
        } write;

        struct {
            char filename[32];
            int flags;
            int exist;      // 1: true, 0: false
            int directory;  // 1: true, 0: false
            int fd;
        } open;

        struct {
            int w, h, x, y;
            char title[16];
        } openwindow;

        struct {
            int layerid;
        } layerid;

        struct {
            int layer_id, x, y, w, h;
            int draw;  // 1: true, 0: false
            uint32_t color;
        } winfillrectangle;

        struct {
            int layer_id, x, y;
            int draw;  // 1: true, 0: false
            uint32_t color;
            char c;
        } winwritechar;

        struct {
            int layer_id, x0, y0, x1, y1;
            int draw;  // 1: true, 0: false
            uint32_t color;
        } windrawline;

        struct {
            int layer_id, x0, y0, rx0, ry0, rx1, ry1;
            int draw;  // 1: true, 0: false
        } winmoverec;

        struct {
            char filename[32];
            char arg[32];
            uint64_t id;
            int exist;      // 1: true, 0: false
            int directory;  // 1: true, 0: false
        } executefile;

        struct {
            uint64_t id;
        } createtask;

    } arg;
};