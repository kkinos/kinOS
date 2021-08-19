/**
 * @file mikanos.cpp
 *
 * MikanOS版のOSサーバー
 */

#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"

extern "C" int main() {
    DrawDesktop();
    InitializeConsole();
    console->PutString("\n");
    console->PutString("unko\n");
    console->PutString("chinchin\n");
    exit(0);
    
}