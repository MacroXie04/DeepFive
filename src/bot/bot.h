#ifndef BOT_H
#define BOT_H

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../core/board.h"

enum class BotMode { Instant, Auto, Thinking, ExtendedThinking, Pro };

class GomokuBot {
   public:
    explicit GomokuBot();
    ~GomokuBot();

    void setMode(BotMode mode);
    BotMode getMode() const;

    // winRate, simulations, elapsedSeconds, progress(0-1)
    void setSearchCallback(std::function<void(double, int, double, double)> cb);

    std::optional<Move> chooseMove(const Board& board, Player side);

    // Analysis Mode
    void startAnalysis(const Board& board, Player side,
                       std::function<void(double, int, double)> cb);
    void stopAnalysis();

    // Returns Simulations Per Second (SPS)
    int runBenchmark(int durationMs);

   private:
    BotMode currentMode;
    std::function<void(double, int, double, double)> statusCallback;

    std::atomic<bool> analysisRunning{false};
    std::atomic<bool> stopAnalysisFlag{false};
    std::thread analysisThread;
};

#endif
