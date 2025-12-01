#include "evaluation.h"

#include <algorithm>
#include <cmath>

namespace Evaluation {

// ========== Position Value Table ==========
// Values favor center positions (higher is better)
static const int POSITION_VALUES[15][15] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0}, {0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0}, {0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 5, 6, 6, 6, 5, 4, 3, 2, 1, 0}, {0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 5, 6, 6, 6, 5, 4, 3, 2, 1, 0}, {0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0}, {0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0},
    {0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0}, {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

int getPositionValue(int row, int col, int boardSize) {
    if (boardSize == 15 && row >= 0 && row < 15 && col >= 0 && col < 15) {
        return POSITION_VALUES[row][col] * 5;
    }
    // For other board sizes, compute dynamically
    int center = boardSize / 2;
    int distFromCenter = std::max(std::abs(row - center), std::abs(col - center));
    return std::max(0, (center - distFromCenter)) * 5;
}

int evaluatePosition(const Board& board, int row, int col, Player player) {
    return Patterns::countPatternScore(board, row, col, player);
}

std::vector<ScoredMove> getScoredMoves(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;

    // Get candidate moves
    std::vector<Move> candidates;
    candidates.reserve(50);
    int size = board.size();

    int minR = size, maxR = -1, minC = size, maxC = -1;
    bool empty = true;
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (!board.isEmpty(r, c)) {
                empty = false;
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
        }
    }

    if (empty) {
        candidates.push_back({size / 2, size / 2, side});
    } else {
        minR = std::max(0, minR - 2);
        maxR = std::min(size - 1, maxR + 2);
        minC = std::max(0, minC - 2);
        maxC = std::min(size - 1, maxC + 2);

        for (int r = minR; r <= maxR; ++r) {
            for (int c = minC; c <= maxC; ++c) {
                if (board.isEmpty(r, c)) {
                    bool hasNeighbor = false;
                    for (int dr = -2; dr <= 2 && !hasNeighbor; ++dr) {
                        for (int dc = -2; dc <= 2; ++dc) {
                            int nr = r + dr;
                            int nc = c + dc;
                            if (board.isInside(nr, nc) && !board.isEmpty(nr, nc)) {
                                hasNeighbor = true;
                                break;
                            }
                        }
                    }
                    if (hasNeighbor) {
                        candidates.push_back({r, c, side});
                    }
                }
            }
        }
    }

    std::vector<ScoredMove> scored;
    scored.reserve(candidates.size());

    for (const auto& mv : candidates) {
        ScoredMove sm;
        sm.move = mv;
        sm.attackScore = Patterns::countPatternScore(board, mv.row, mv.col, side);
        sm.defenseScore = (int)(Patterns::countPatternScore(board, mv.row, mv.col, opp) * 1.2);
        sm.positionScore = getPositionValue(mv.row, mv.col, board.size());
        scored.push_back(sm);
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.totalScore() > b.totalScore();
    });

    return scored;
}

BoardEvaluation evaluateBoard(const Board& board, Player currentPlayer) {
    BoardEvaluation eval = {0, 0, 0, 0, 0.5f};
    int size = board.size();

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (board.isEmpty(r, c)) {
                bool hasNeighbor = false;
                for (int dr = -2; dr <= 2 && !hasNeighbor; ++dr) {
                    for (int dc = -2; dc <= 2; ++dc) {
                        int nr = r + dr;
                        int nc = c + dc;
                        if (board.isInside(nr, nc) && !board.isEmpty(nr, nc)) {
                            hasNeighbor = true;
                            break;
                        }
                    }
                }

                if (hasNeighbor) {
                    int blackScore = Patterns::countPatternScore(board, r, c, Player::Black);
                    int whiteScore = Patterns::countPatternScore(board, r, c, Player::White);

                    eval.blackScore += std::min(blackScore, Patterns::SCORE_LIVE_FOUR);
                    eval.whiteScore += std::min(whiteScore, Patterns::SCORE_LIVE_FOUR);

                    if (blackScore >= Patterns::SCORE_LIVE_THREE) eval.blackThreats++;
                    if (whiteScore >= Patterns::SCORE_LIVE_THREE) eval.whiteThreats++;
                }
            }
        }
    }

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (!board.isEmpty(r, c)) {
                int posVal = getPositionValue(r, c, size);
                if (board.at(r, c) == Player::Black) {
                    eval.blackScore += posVal;
                } else {
                    eval.whiteScore += posVal;
                }
            }
        }
    }

    double scoreDiff = (currentPlayer == Player::Black) ? (eval.blackScore - eval.whiteScore)
                                                        : (eval.whiteScore - eval.blackScore);

    double scaleFactor = 0.0001;
    eval.winProbability = (float)(1.0 / (1.0 + std::exp(-scoreDiff * scaleFactor)));

    return eval;
}

double getHeuristicBias(const Board& board, const Move& move) {
    if (!board.isEmpty(move.row, move.col)) return 0.0;

    Player opp = (move.player == Player::Black) ? Player::White : Player::Black;

    int attackScore = Patterns::fastHeuristicScore(board, move.row, move.col, move.player);
    int defenseScore = Patterns::fastHeuristicScore(board, move.row, move.col, opp);
    int positionScore = getPositionValue(move.row, move.col, board.size());

    int totalScore = attackScore + (int)(defenseScore * 1.2) + positionScore;

    double normalized = totalScore / 50000.0;
    double bias = 1.0 / (1.0 + std::exp(-normalized * 3.0));

    return bias * 0.5;
}

}  // namespace Evaluation
