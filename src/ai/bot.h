#ifndef BOT_H
#define BOT_H

#include "../core/board.h"
#include <optional>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

enum class BotMode {
    Instant,
    Auto,
    Thinking,
    ExtendedThinking,
    Pro
};

class GomokuBot {
public:
    explicit GomokuBot();
    ~GomokuBot();
    
    void setMode(BotMode mode);
    BotMode getMode() const;

    // winRate, simulations, elapsedSeconds, progress(0-1)
    void setSearchCallback(std::function<void(double, int, double, double)> cb);

    std::optional<Move> chooseMove(const Board& board, Player side);
    
    // Exposed for search helpers that need the same move ordering heuristics
    static std::vector<Move> getCandidateMoves(const Board& board, Player side);
    
    // Analysis Mode
    void startAnalysis(const Board& board, Player side, std::function<void(double, int, double)> cb);
    void stopAnalysis();

    // Returns Simulations Per Second (SPS)
    int runBenchmark(int durationMs);

private:
    BotMode currentMode;
    std::function<void(double, int, double, double)> statusCallback;
    
    std::atomic<bool> analysisRunning{false};
    std::atomic<bool> stopAnalysisFlag{false};
    std::thread analysisThread;

    // MCTS Helpers
    // Returns optional move and total simulations
    std::pair<std::optional<Move>, int> runMCTS(const Board& board, Player side, int durationMs);
    
    // Heuristic Helpers
    // Check for immediate win (5) or forced block (4)
    std::optional<Move> checkImmediateThreats(const Board& board, Player side);
};

#endif
