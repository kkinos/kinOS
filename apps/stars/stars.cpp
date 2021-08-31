#include <cstdlib>
#include <random>
#include "../../libs/mikanos/mikansyscall.hpp"

static constexpr int kWidth = 100, kHeight = 100;

extern "C" void main(int argc, char** argv) {
  int layer_id = OpenWindow(kWidth + 8, kHeight + 28, 10, 10);
  if (layer_id == -1) {
    exit(1);
  }

 WinFillRectangle(layer_id, false,
                  4, 24, kWidth, kHeight, 0x000000);

  int num_stars = 100;
  if (argc >= 2) {
    num_stars = atoi(argv[1]);
  }

  auto [tick_start, timer_freq] = SyscallGetCurrentTick();

  std::default_random_engine rand_engine;
  std::uniform_int_distribution x_dist(0, kWidth - 2), y_dist(0, kHeight - 2);
  for (int i = 0; i < num_stars; ++i) {
    int x = x_dist(rand_engine);
    int y = y_dist(rand_engine);
    WinFillRectangle(layer_id, false,
                      4 + x, 24 + y, 2, 2, 0xfff100);
  }
  WinRedraw(layer_id);

  auto tick_end = SyscallGetCurrentTick();
  printf("%d stars in %lu ms.\n",
         num_stars,
         (tick_end.value - tick_start) * 1000 / timer_freq);

  exit(0);
}