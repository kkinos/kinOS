/**
 * @file mikansyscall.hpp
 *
 * MikanOSサーバが提供するシステムコール
 */

#pragma once

#include "../common/message.hpp"
#include "../kinos/syscall.h"

/**
 * @brief ウィンドウを作成する 返り値はlayer_idでウィンドウを操作するような関数はこれを用いる -1はエラー
 * 
 * @param w 
 * @param h 
 * @param x 
 * @param y 
 * @return int layer_id 
 */
int OpenWindow(int w, int h, int x, int y);

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
void WinFillRectangle(int layer_id, bool draw, int x, int y, int w, int h, uint32_t color);

void WinWriteChar(int layer_id, bool draw, int x, int y, uint32_t color, char c);

void WinWriteString(int layer_id, bool draw, int x, int y, uint32_t color, char *s);

void WinDrawLine(int layer_id, bool draw, int x0, int y0, int x1, int y1, uint32_t color);

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
void WinMoveRec(int layer_id, bool draw, int x0, int y0, int rx0, int ry0, int rx1, int ry1);

void WinRedraw(int layer_id);

void CloseWindow(int layer_id);
