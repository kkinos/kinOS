#include "graphics.hpp"
#include "font.hpp"

extern "C" int main() {
    DrawDesktop();
    WriteString({ 66, 66 }, "Hello, world!", {0, 0, 255});
    exit(0);
    
}