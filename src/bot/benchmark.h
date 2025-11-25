#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <chrono>
#include <iostream>
#include <string>

#include "../core/board.h"
#include "bot.h"

class Benchmark {
   public:
    // Returns Simulations Per Second (SPS)
    static int run(GomokuBot& bot) {
        Board board(15);

        board.placeStone(7, 7, Player::Black);

        return bot.runBenchmark(1000);
    }
};

#endif
