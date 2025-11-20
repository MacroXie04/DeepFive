#ifndef BOT_H
#define BOT_H

#include "../core/board.h"
#include <optional>
#include <functional>
#include <string>
#include <vector>
#include <memory>

enum class BotMode {
    Instant,
    Auto,
    Thinking,
    ExtendedThinking
};

class GomokuBot {
public:
    explicit GomokuBot();
    
    void setMode(BotMode mode);
    BotMode getMode() const;

    // winRate, simulations, elapsedSeconds
    void setSearchCallback(std::function<void(double, int, double)> cb);

    std::optional<Move> chooseMove(const Board& board, Player side);
    
    // Returns Simulations Per Second (SPS)
    int runBenchmark(int durationMs);

private:
    BotMode currentMode;
    std::function<void(double, int, double)> statusCallback;

    // MCTS Helpers
    // Returns optional move and total simulations
    std::pair<std::optional<Move>, int> runMCTS(const Board& board, Player side, int durationMs);
    
    // Heuristic Helpers
    std::vector<Move> getCandidateMoves(const Board& board, Player side);
    // Check for immediate win (5) or forced block (4)
    std::optional<Move> checkImmediateThreats(const Board& board, Player side);
};

#endif
