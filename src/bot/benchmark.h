#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "bot.h"
#include "../core/board.h"
#include <chrono>
#include <iostream>
#include <string>

class Benchmark {
public:
    // Returns Simulations Per Second (SPS)
    static int run(GomokuBot& bot) {
        Board board(15);
        // Place one stone to avoid empty board edge cases and make it slightly realistic
        board.placeStone(7, 7, Player::Black);
        
        // Temporarily override bot callback to silence it during benchmark? 
        // Or just let it run.
        // We want to measure raw MCTS speed.
        
        // We can't easily hook into private `runMCTS` directly unless we are friend or add public benchmark method to Bot.
        // For clean design, let's add a `runBenchmark(durationMs)` method to GomokuBot.
        return bot.runBenchmark(1000); // Run for 1 second
    }
};

#endif

