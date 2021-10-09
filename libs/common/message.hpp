#pragma once

#include <stdint.h>

enum class LayerOperation { Move, MoveRelative, Draw, DrawArea };

struct Message {
    enum Type {
        /*--------------------------------------------------------------------------
         * common message types
         *--------------------------------------------------------------------------
         */
        Error,
        Ready,
        /*--------------------------------------------------------------------------
         * message types to use kernel functions
         *--------------------------------------------------------------------------
         */
        kInterruptXHCI,
        kTimerTimeout,
        kKeyPush,
        kLayer,
        kLayerFinish,
        kMouseMove,
        kMouseButton,
        kWindowActive,
        kPipe,
        kExpandTaskBuffer,
        kExcute,
        /*--------------------------------------------------------------------------
         * message types for servers and applications
         *--------------------------------------------------------------------------
         */
        aMouseMove,
        aKeyPush,
        aOpenWindow,
        aLayerId,
        aWinFillRectangle,
        aWinWriteChar,
        aWinRedraw,
        aQuit,
        aCloseWindow,
        aMouseButton,
        aWinDrawLine,
        aWinMoveRec,
        aFindFile,
        aCreateTask,
        aExecuteFile,
    } type;

    uint64_t src_task;

    union {
        struct {
            bool retry;
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
            bool success;
        } execute;

        struct {
            uint64_t id;
            uint32_t bytes;
        } expand;

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
            bool draw;
            uint32_t color;
            char c;
        } winwritechar;

        struct {
            int layer_id, x0, y0, x1, y1;
            bool draw;
            uint32_t color;
        } windrawline;

        struct {
            int layer_id, x0, y0, rx0, ry0, rx1, ry1;
            bool draw;
        } winmoverec;

        struct {
            char filename[16];
            bool exist;
            bool directory;
        } findfile;

        struct {
            char filename[16];
            char arg[32];
            uint64_t id;
            bool exist;
            bool directory;
        } executefile;

        struct {
            uint64_t id;
        } createtask;

    } arg;
};