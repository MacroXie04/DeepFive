#ifndef MCTS_H
#define MCTS_H

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
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
    double heuristicBias;  // Progressive bias from heuristic evaluation
    int heuristicScore;
    uint64_t stateHash;

    MCTSNode(Move m, MCTSNode* p, Player justMoved, double bias = 0.0, int score = 0,
             uint64_t hash = 0);

    bool isFullyExpanded() const;
    // UCT with optional progressive bias
    MCTSNode* bestChild(double explorationValue = 1.41, bool useProgressiveBias = true);
};

class MCTSSolver {
   public:
    struct Options {
        int rootBeamSize = 40;
        int childBeamSize = 24;
        int rolloutSampleSize = 8;
        int rolloutDepthLimit = 100;
        bool enableTranspositionTable = true;
        std::optional<uint32_t> seed;
    };

    MCTSSolver(const Board& board, Player side);
    MCTSSolver(const Board& board, Player side, Options options);

    void setSeed(uint32_t seed);

    // Run for a fixed duration (ms)
    // callback: (winRate%, simulations, elapsedSec, progress0-1)
    void run(int durationMs, std::function<void(double, int, double, double)> statusCb = nullptr);

    // Run until stopped
    // callback: (winRate%, simulations, elapsedSec)
    void runContinuous(std::atomic<bool>& stopFlag,
                       std::function<void(double, int, double)> statusCb);

    // Run a fixed number of iterations, primarily for deterministic tests and benchmarks.
    void runIterations(int iterations);

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
    Options options;
    std::mt19937 rng;
    int totalSimulations = 0;

    struct TranspositionEntry {
        bool hasCandidates = false;
        std::vector<Move> candidates;
        bool hasStaticEval = false;
        double blackWinProbability = 0.5;
        int visits = 0;
        double wins = 0.0;
    };
    std::unordered_map<uint64_t, TranspositionEntry> transpositionTable;

    // Perform one iteration of MCTS
    void step();
    uint64_t getTranspositionKey(uint64_t boardHash, Player side) const;
    std::vector<Move> getOrderedMoves(const Board& board, Player side, int maxMoves);
    double evaluateStaticBlackWinProbability(const Board& board);
    std::optional<Move> getRolloutMove(const Board& board, Player side);
};

#endif
