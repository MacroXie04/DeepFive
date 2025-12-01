#ifndef EVALUATION_H
#define EVALUATION_H

#include <vector>

#include "../core/board.h"
#include "patterns.h"

namespace Evaluation {

// Move with evaluation score
struct ScoredMove {
    Move move;
    int attackScore;   // Score for our attack potential
    int defenseScore;  // Score for blocking opponent
    int positionScore; // Score for position value
    int totalScore() const { return attackScore + defenseScore + positionScore; }
};

// Board evaluation result
struct BoardEvaluation {
    int blackScore;
    int whiteScore;
    int blackThreats;   // Number of live threes and above
    int whiteThreats;
    float winProbability;  // Estimated win probability for current player
};

// Position value for a cell (center positions are more valuable)
int getPositionValue(int row, int col, int boardSize = 15);

// Evaluate a single position's pattern score for a player
int evaluatePosition(const Board& board, int row, int col, Player player);

// Returns candidate moves with evaluation scores, sorted by score (best first)
std::vector<ScoredMove> getScoredMoves(const Board& board, Player side);

// Global board evaluation - considers all patterns on the board
BoardEvaluation evaluateBoard(const Board& board, Player currentPlayer);

// Get heuristic bias score for MCTS (normalized 0-1)
double getHeuristicBias(const Board& board, const Move& move);

}  // namespace Evaluation

#endif

