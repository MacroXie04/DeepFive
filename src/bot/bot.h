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

enum class BotMode { Instant, Auto, Thinking, Pro };

// Algorithm stages used by the bot
enum class AlgorithmStage {
    Tengen,        // Opening move at center
    DirectWin,     // Immediate winning move
    BlockWin,      // Block opponent's winning move
    DoubleThreat,  // Create double threat
    BlockThreat,   // Block opponent's double threat
    VCF,           // Victory by Continuous Fours
    BlockVCF,      // Block opponent's VCF
    VCT,           // Victory by Continuous Threes
    MCTS           // Monte Carlo Tree Search
};

// Get display name for algorithm stage
inline const char* getAlgorithmStageName(AlgorithmStage stage) {
    switch (stage) {
        case AlgorithmStage::Tengen:
            return "Opening (Tengen)";
        case AlgorithmStage::DirectWin:
            return "Winning Move";
        case AlgorithmStage::BlockWin:
            return "Block Win";
        case AlgorithmStage::DoubleThreat:
            return "Double Threat";
        case AlgorithmStage::BlockThreat:
            return "Block Threat";
        case AlgorithmStage::VCF:
            return "VCF Search";
        case AlgorithmStage::BlockVCF:
            return "Block VCF";
        case AlgorithmStage::VCT:
            return "VCT Search";
        case AlgorithmStage::MCTS:
            return "MCTS";
        default:
            return "Unknown";
    }
}

class GomokuBot {
   public:
    explicit GomokuBot();
    ~GomokuBot();

    void setMode(BotMode mode);
    BotMode getMode() const;

    // Get display name for current bot mode
    const char* getModeName() const;

    void setSelfPlayMode(bool enabled);
    bool isSelfPlayMode() const;

    // Get the algorithm stage used for the last move
    AlgorithmStage getLastAlgorithmStage() const { return lastAlgorithmStage; }

    // winRate, simulations, elapsedSeconds, progress(0-1)
    void setSearchCallback(std::function<void(double, int, double, double)> cb);

    // Candidate evaluation callback for visualization
    // vector of (row, col, player, score)
    using CandidateCallback =
        std::function<void(const std::vector<std::tuple<int, int, Player, float>>&)>;
    void setCandidateCallback(CandidateCallback cb);

    std::optional<Move> chooseMove(const Board& board, Player side);

    // Analysis Mode
    void startAnalysis(const Board& board, Player side,
                       std::function<void(double, int, double)> cb);
    void stopAnalysis();

    // Returns Simulations Per Second (SPS)
    int runBenchmark(int durationMs);

   private:
    BotMode currentMode;
    bool selfPlayMode = false;
    AlgorithmStage lastAlgorithmStage = AlgorithmStage::MCTS;
    std::function<void(double, int, double, double)> statusCallback;
    CandidateCallback candidateCallback;

    std::atomic<bool> analysisRunning{false};
    std::atomic<bool> stopAnalysisFlag{false};
    std::thread analysisThread;
};

#endif
