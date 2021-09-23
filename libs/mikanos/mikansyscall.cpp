#include "mikansyscall.hpp"

/**
 * @brief ウィンドウを作成する 返り値はlayer_idでウィンドウを操作するような関数はこれを用いる -1はエラー
 * 
 * @param w 
 * @param h 
 * @param x 
 * @param y 
 * @return int layer_id 
 */
int OpenWindow(int w, int h, int x, int y)
{
    auto [id, err1] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
        return -1;
    }
    else
    {
        Message msg{Message::aOpenWindow};
        msg.arg.openwindow.w = w;
        msg.arg.openwindow.h = h;
        msg.arg.openwindow.x = x;
        msg.arg.openwindow.y = y;
        SyscallSendMessage(&msg, id);
        Message rmsg[1];
        int layer_id;
        while (true)
        {
            auto [n, err2] = SyscallReceiveMessage(rmsg, 1);
            if (err2)
            {
                layer_id = -1;
                break;
            }

            if (rmsg[0].type == Message::aLayerId)
            {
                layer_id = rmsg[0].arg.layerid.layerid;
                break;
            }
        }
        return layer_id;
    }
}

/**
 * @brief ウィンドウ内で中を埋めた四角形を表示する
 * 
 * @param layer_id 
 * @param x 
 * @param y 
 * @param w 
 * @param h 
 * @param draw trueなら描画する falseならしない
 * @param color 
 */
void WinFillRectangle(int layer_id, bool draw, int x, int y, int w, int h, uint32_t color)
{
    auto [id, err] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
    }
    else
    {
        Message msg{Message::aWinFillRectangle};
        msg.arg.winfillrectangle.layer_id = layer_id;
        msg.arg.winfillrectangle.draw = draw;
        msg.arg.winfillrectangle.x = x;
        msg.arg.winfillrectangle.y = y;
        msg.arg.winfillrectangle.w = w;
        msg.arg.winfillrectangle.h = h;
        msg.arg.winfillrectangle.color = color;
        SyscallSendMessage(&msg, id);
    }
}

void WinWriteChar(int layer_id, bool draw, int x, int y, uint32_t color, char c)
{
    auto [id, err] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
    }
    else
    {
        Message msg{Message::aWinWriteChar};
        msg.arg.winwritechar.layer_id = layer_id;
        msg.arg.winwritechar.draw = draw;
        msg.arg.winwritechar.x = x;
        msg.arg.winwritechar.y = y;
        msg.arg.winwritechar.color = color;
        msg.arg.winwritechar.c = c;
        SyscallSendMessage(&msg, id);
    }
}

void WinWriteString(int layer_id, bool draw, int x, int y, uint32_t color, char *s)
{
    int x_c = 0;
    while (*s)
    {
        WinWriteChar(layer_id, draw, x + x_c, y, color, *s);
        ++s;
        x_c = x_c + 8;
    }
}

void WinDrawLine(int layer_id, bool draw, int x0, int y0, int x1, int y1, uint32_t color)
{
    auto [id, err] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
    }
    else
    {
        Message msg{Message::aWinDrawLine};
        msg.arg.windrawline.layer_id = layer_id;
        msg.arg.windrawline.draw = draw;
        msg.arg.windrawline.x0 = x0;
        msg.arg.windrawline.y0 = y0;
        msg.arg.windrawline.x1 = x1;
        msg.arg.windrawline.y1 = y1;
        msg.arg.windrawline.color = color;
        SyscallSendMessage(&msg, id);
    }
}

/**
 * @brief ウィンドウ内の領域を移動させる ウィンドウの左上を0とする
 * 
 * @param layer_id 
 * @param draw 
 * @param x0 移動させたい位置
 * @param y0 
 * @param rx0 移動させたい領域
 * @param ry0 
 * @param rx1 
 * @param ry1 
 */
void WinMoveRec(int layer_id, bool draw, int x0, int y0, int rx0, int ry0, int rx1, int ry1)
{
    auto [id, err] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
    }
    else
    {
        Message msg{Message::aWinMoveRec};
        msg.arg.winmoverec.layer_id = layer_id;
        msg.arg.winmoverec.draw = draw;
        msg.arg.winmoverec.x0 = x0;
        msg.arg.winmoverec.y0 = y0;
        msg.arg.winmoverec.rx0 = rx0;
        msg.arg.winmoverec.ry0 = ry0;
        msg.arg.winmoverec.rx1 = rx1;
        msg.arg.winmoverec.ry1 = ry1;
        SyscallSendMessage(&msg, id);
    }
}

void WinRedraw(int layer_id)
{
    auto [id, err] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
    }
    else
    {
        Message msg{Message::aWinRedraw};
        msg.arg.layerid.layerid = layer_id;
        SyscallSendMessage(&msg, id);
    }
}

void CloseWindow(int layer_id)
{
    auto [id, err] = SyscallFindServer("servers/mikanos");
    if (id == -1)
    {
    }
    else
    {
        Message msg{Message::aCloseWindow};
        msg.arg.layerid.layerid = layer_id;
        SyscallSendMessage(&msg, id);
    }
}