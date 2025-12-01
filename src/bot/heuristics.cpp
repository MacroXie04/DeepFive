#include "heuristics.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace Heuristics {

// Thread-local random generator
static thread_local std::mt19937 rng(std::random_device{}());

// ========== Move Generation ==========

std::vector<Move> getCandidateMoves(const Board& board, Player side) {
    std::vector<Move> moves;
    moves.reserve(50);
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
        moves.push_back({size / 2, size / 2, side});
        return moves;
    }

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
                    moves.push_back({r, c, side});
                }
            }
        }
    }

    return moves;
}

std::optional<Move> getFastRandomMove(const Board& board, Player side) {
    int size = board.size();

    static thread_local std::vector<std::pair<int, int>> stones;
    stones.clear();
    stones.reserve(225);

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (!board.isEmpty(r, c)) {
                stones.push_back({r, c});
            }
        }
    }

    if (stones.empty()) {
        return Move{size / 2, size / 2, side};
    }

    std::shuffle(stones.begin(), stones.end(), rng);

    int drs[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dcs[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int indices[] = {0, 1, 2, 3, 4, 5, 6, 7};

    for (const auto& s : stones) {
        int r = s.first;
        int c = s.second;

        for (int i = 0; i < 8; ++i) {
            int j = std::uniform_int_distribution<>(i, 7)(rng);
            std::swap(indices[i], indices[j]);
        }

        for (int idx : indices) {
            int nr = r + drs[idx];
            int nc = c + dcs[idx];
            if (board.isInside(nr, nc) && board.isEmpty(nr, nc)) {
                return Move{nr, nc, side};
            }
        }
    }

    return std::nullopt;
}

// ========== Threat Detection Helpers ==========

static int countDirection(const Board& board, int r, int c, int dr, int dc, Player player) {
    int count = 0;
    int nr = r + dr;
    int nc = c + dc;
    while (board.isInside(nr, nc) && board.at(nr, nc) == player) {
        count++;
        nr += dr;
        nc += dc;
    }
    return count;
}

static bool wouldWin(const Board& board, int r, int c, Player player) {
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (auto& dir : directions) {
        int count = 1;
        count += countDirection(board, r, c, dir[0], dir[1], player);
        count += countDirection(board, r, c, -dir[0], -dir[1], player);
        if (count >= 5) return true;
    }
    return false;
}

// ========== Threat Detection ==========

std::optional<Move> getFastThreatMove(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    int size = board.size();

    int minR = size, maxR = -1, minC = size, maxC = -1;
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (!board.isEmpty(r, c)) {
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
        }
    }

    if (maxR < 0) return std::nullopt;

    minR = std::max(0, minR - 1);
    maxR = std::min(size - 1, maxR + 1);
    minC = std::max(0, minC - 1);
    maxC = std::min(size - 1, maxC + 1);

    // First pass: can we win?
    for (int r = minR; r <= maxR; ++r) {
        for (int c = minC; c <= maxC; ++c) {
            if (board.isEmpty(r, c) && wouldWin(board, r, c, side)) {
                return Move{r, c, side};
            }
        }
    }

    // Second pass: must we block opponent's win?
    for (int r = minR; r <= maxR; ++r) {
        for (int c = minC; c <= maxC; ++c) {
            if (board.isEmpty(r, c) && wouldWin(board, r, c, opp)) {
                return Move{r, c, side};
            }
        }
    }

    return std::nullopt;
}

std::optional<Move> checkImmediateThreats(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    auto candidates = getCandidateMoves(board, side);

    // Can we win directly?
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, side);
        if (temp.checkWinner() == side) {
            return move;
        }
    }

    // Must we block opponent win?
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, opp);
        if (temp.checkWinner() == opp) {
            return Move{move.row, move.col, side};
        }
    }

    return std::nullopt;
}

std::optional<Move> findDoubleThreat(const Board& board, Player side) {
    auto candidates = getCandidateMoves(board, side);

    for (const auto& mv : candidates) {
        auto threats = Patterns::countThreats(board, mv.row, mv.col, side);

        // Check for winning combinations
        if (threats.liveFours >= 1) return mv;
        if (threats.totalFours() >= 2) return mv;
        if (threats.totalFours() >= 1 && threats.totalThrees() >= 1) return mv;
        if (threats.totalThrees() >= 2) return mv;
    }

    return std::nullopt;
}

// ========== Move Selection ==========

std::optional<Move> getBestHeuristicMove(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;

    // 1. Can we win directly?
    for (const auto& mv : getCandidateMoves(board, side)) {
        if (wouldWin(board, mv.row, mv.col, side)) {
            return mv;
        }
    }

    // 2. Must block opponent's win?
    for (const auto& mv : getCandidateMoves(board, side)) {
        if (wouldWin(board, mv.row, mv.col, opp)) {
            return mv;
        }
    }

    // 3. Can we create a double threat?
    if (auto doubleThreat = findDoubleThreat(board, side)) {
        return doubleThreat;
    }

    // 4. Must block opponent's double threat?
    if (auto oppDoubleThreat = findDoubleThreat(board, opp)) {
        return Move{oppDoubleThreat->row, oppDoubleThreat->col, side};
    }

    // 5. Get best scored move
    auto scoredMoves = Evaluation::getScoredMoves(board, side);
    if (!scoredMoves.empty()) {
        int topCount = std::min(3, (int)scoredMoves.size());
        int bestScore = scoredMoves[0].totalScore();
        int threshold = (int)(bestScore * 0.8);
        topCount = 0;
        for (const auto& sm : scoredMoves) {
            if (sm.totalScore() >= threshold)
                topCount++;
            else
                break;
        }

        if (topCount > 1) {
            std::uniform_int_distribution<> dis(0, topCount - 1);
            return scoredMoves[dis(rng)].move;
        }

        return scoredMoves[0].move;
    }

    return std::nullopt;
}

std::optional<Move> getEvaluationGuidedMove(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    auto candidates = getCandidateMoves(board, side);

    if (candidates.empty()) {
        return getFastRandomMove(board, side);
    }

    std::vector<std::pair<Move, int>> scored;
    scored.reserve(candidates.size());

    for (const auto& mv : candidates) {
        int attack = Patterns::fastHeuristicScore(board, mv.row, mv.col, side);
        int defense = Patterns::fastHeuristicScore(board, mv.row, mv.col, opp);
        int total = attack + (int)(defense * 1.2);
        scored.push_back({mv, total});
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    if (scored[0].second >= 100000) {
        return scored[0].first;
    }

    int maxMoves = std::min(5, (int)scored.size());
    std::vector<double> weights;
    double totalWeight = 0.0;

    for (int i = 0; i < maxMoves; ++i) {
        double weight = std::exp(scored[i].second / 2000.0);
        weights.push_back(weight);
        totalWeight += weight;
    }

    if (totalWeight == 0) {
        return scored[0].first;
    }

    std::uniform_real_distribution<> dis(0.0, totalWeight);
    double sample = dis(rng);
    double cumulative = 0.0;

    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (sample <= cumulative) {
            return scored[i].first;
        }
    }

    return scored[0].first;
}

}  // namespace Heuristics
