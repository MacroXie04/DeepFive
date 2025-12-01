#ifndef HEURISTICS_H
#define HEURISTICS_H

#include <optional>
#include <vector>

#include "../core/board.h"

namespace Heuristics {

// Pattern types with scores
enum class Pattern {
    FIVE = 0,       // 五连 - 已获胜
    LIVE_FOUR,      // 活四 - 必胜
    RUSH_FOUR,      // 冲四 - 对方必挡
    LIVE_THREE,     // 活三 - 强威胁
    SLEEP_THREE,    // 眠三 - 潜在威胁
    LIVE_TWO,       // 活二 - 发展潜力
    SLEEP_TWO,      // 眠二
    NONE
};

// Pattern scores for evaluation
constexpr int SCORE_FIVE = 100000;
constexpr int SCORE_LIVE_FOUR = 50000;
constexpr int SCORE_RUSH_FOUR = 5000;
constexpr int SCORE_LIVE_THREE = 3000;
constexpr int SCORE_SLEEP_THREE = 500;
constexpr int SCORE_LIVE_TWO = 100;
constexpr int SCORE_SLEEP_TWO = 10;

// Move with evaluation score
struct ScoredMove {
    Move move;
    int attackScore;   // Score for our attack potential
    int defenseScore;  // Score for blocking opponent
    int totalScore() const { return attackScore + defenseScore; }
};

// Returns plausible moves to consider for MCTS or other searches
std::vector<Move> getCandidateMoves(const Board& board, Player side);

// Returns candidate moves with evaluation scores, sorted by score (best first)
std::vector<ScoredMove> getScoredMoves(const Board& board, Player side);

// Evaluate a single position's pattern score for a player
int evaluatePosition(const Board& board, int row, int col, Player player);

// Count patterns at a position in all directions
int countPatternScore(const Board& board, int row, int col, Player player);

// Check for double threats (e.g., 双活三, 冲四活三)
// Returns the move that creates double threat, or nullopt
std::optional<Move> findDoubleThreat(const Board& board, Player side);

// Returns a random move near existing stones (fast rollout policy)
std::optional<Move> getFastRandomMove(const Board& board, Player side);

// Check for immediate win (5) or forced block (4) - SLOW, don't use in rollout
std::optional<Move> checkImmediateThreats(const Board& board, Player side);

// Fast threat check for rollout - no board copies, O(1) per position
// Returns winning move or blocking move if found
std::optional<Move> getFastThreatMove(const Board& board, Player side);

// Smart move selection using all heuristics
// Returns the best move considering patterns, threats, and double threats
std::optional<Move> getBestHeuristicMove(const Board& board, Player side);

}  // namespace Heuristics

#endif
