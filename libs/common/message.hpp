#pragma once

#include <errno.h>
#include <stdint.h>

struct Message {
    enum Type {
        /*--------------------------------------------------------------------------
         * common message types
         *--------------------------------------------------------------------------
         */
        kError,
        kReady,
        kReceived,
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
        kStartServer,
        kStartApp,
        kExitServer,
        kExitApp,
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
        kOpenDir,
        kRead,
        kCreateTask,
        kExecuteFile,
        kRedirect,
    } type;

    uint64_t src_task;

    union {
        struct {
            int retry;  // 1: true, 0: false
            int err;    // errno
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
        } starttask;

        struct {
            uint64_t id;
            uint32_t bytes;
        } expand;

        struct {
            uint64_t id;
            uint64_t result;
        } exitapp;

        struct {
            char name[32];
        } exitserver;

        struct {
            char filename[32];
            int flags;
            int exist;        // 1: true, 0: false
            int isdirectory;  // 1: true, 0: false
            int fd;
        } open;

        struct {
            char dirname[32];
            int exist;        // 1: true, 0: false
            int isdirectory;  // 1: true, 0: false
            int fd;
        } opendir;

        struct {
            char filename[32];
            int fd;
            char data[16];
            uint8_t len;
            uint32_t count;
            uint32_t offset;
            uint32_t cluster;  // use for directory
        } read;

        struct {
            char filename[32];
            int fd;
            char data[16];
            uint8_t len;
            uint32_t count;
            uint32_t offset;
            uint32_t cluster;
        } write;

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
            int redirect;     // 1: true, 0: false
            uint64_t id;      // target task id
            int exist;        // 1: true, 0: false
            int isdirectory;  // 1: true, 0: false
        } executefile;

        struct {
            char filename[32];
        } redirect;

        struct {
            uint64_t id;
        } createtask;

    } arg;
};