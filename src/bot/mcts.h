#ifndef MCTS_H
#define MCTS_H

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "../core/board.h"
#include "heuristics.h"

struct MCTSNode {
    Move move;
    MCTSNode* parent;
    std::vector<std::unique_ptr<MCTSNode>> children;
    int visits;
    double wins;
    std::vector<Move> untriedMoves;
    Player playerJustMoved;

    MCTSNode(Move m, MCTSNode* p, Player justMoved);

    bool isFullyExpanded() const;
    MCTSNode* bestChild(double explorationValue = 1.41);
};

class MCTSSolver {
   public:
    MCTSSolver(const Board& board, Player side);

    // Run for a fixed duration (ms)
    // callback: (winRate%, simulations, elapsedSec, progress0-1)
    void run(int durationMs, std::function<void(double, int, double, double)> statusCb = nullptr);

    // Run until stopped
    // callback: (winRate%, simulations, elapsedSec)
    void runContinuous(std::atomic<bool>& stopFlag,
                       std::function<void(double, int, double)> statusCb);

    std::pair<std::optional<Move>, int> getBestMove();

    // Get all candidate moves with their evaluation scores (0.0 to 1.0)
    struct CandidateEval {
        Move move;
        float score;  // win rate normalized to 0.0-1.0
        int visits;
    };
    std::vector<CandidateEval> getCandidateEvaluations() const;

   private:
    std::unique_ptr<MCTSNode> root;
    Board rootBoard;
    // Player rootSide; // Unused
    int totalSimulations = 0;

    // Perform one iteration of MCTS
    void step();
};

#endif
