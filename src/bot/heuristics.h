#ifndef HEURISTICS_H
#define HEURISTICS_H

#include <optional>
#include <vector>

#include "../core/board.h"
#include "evaluation.h"
#include "patterns.h"

namespace Heuristics {

// Re-export commonly used types for convenience
using ScoredMove = Evaluation::ScoredMove;
using BoardEvaluation = Evaluation::BoardEvaluation;

// Re-export pattern scores
using Patterns::SCORE_FIVE;
using Patterns::SCORE_LIVE_FOUR;
using Patterns::SCORE_RUSH_FOUR;
using Patterns::SCORE_LIVE_THREE;

// ========== Move Generation ==========

// Returns plausible moves to consider for MCTS or other searches
std::vector<Move> getCandidateMoves(const Board& board, Player side);

// Returns a random move near existing stones (fast rollout policy)
std::optional<Move> getFastRandomMove(const Board& board, Player side);

// ========== Threat Detection ==========

// Check for immediate win (5) or forced block (4)
std::optional<Move> checkImmediateThreats(const Board& board, Player side);

// Fast threat check for rollout - no board copies
std::optional<Move> getFastThreatMove(const Board& board, Player side);

// Check for double threats (双活三, 冲四活三, etc.)
std::optional<Move> findDoubleThreat(const Board& board, Player side);

// ========== Move Selection ==========

// Smart move selection using all heuristics
std::optional<Move> getBestHeuristicMove(const Board& board, Player side);

// Get a move using evaluation-guided sampling (for improved rollout)
std::optional<Move> getEvaluationGuidedMove(const Board& board, Player side);

// ========== Convenience Wrappers ==========

// Wrapper for Patterns::countPatternScore
inline int countPatternScore(const Board& board, int row, int col, Player player) {
    return Patterns::countPatternScore(board, row, col, player);
}

// Wrapper for Evaluation::getScoredMoves
inline std::vector<ScoredMove> getScoredMoves(const Board& board, Player side) {
    return Evaluation::getScoredMoves(board, side);
}

// Wrapper for Evaluation::evaluateBoard
inline BoardEvaluation evaluateBoard(const Board& board, Player currentPlayer) {
    return Evaluation::evaluateBoard(board, currentPlayer);
}

// Wrapper for Evaluation::getHeuristicBias
inline double getHeuristicBias(const Board& board, const Move& move) {
    return Evaluation::getHeuristicBias(board, move);
}

// Wrapper for Evaluation::getPositionValue
inline int getPositionValue(int row, int col, int boardSize = 15) {
    return Evaluation::getPositionValue(row, col, boardSize);
}

}  // namespace Heuristics

#endif
