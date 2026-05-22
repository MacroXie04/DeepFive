#include "heuristics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace Heuristics {

// Thread-local random generator
static thread_local std::mt19937 rng(std::random_device{}());

// ========== Move Generation ==========

void setRandomSeed(uint32_t seed) {
    rng.seed(seed);
}

static Player opponentOf(Player player) {
    return (player == Player::Black) ? Player::White : Player::Black;
}

static std::vector<Move> collectCandidateMoves(const Board& board, Player side, int distance) {
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

    minR = std::max(0, minR - distance);
    maxR = std::min(size - 1, maxR + distance);
    minC = std::max(0, minC - distance);
    maxC = std::min(size - 1, maxC + distance);

    for (int r = minR; r <= maxR; ++r) {
        for (int c = minC; c <= maxC; ++c) {
            if (board.isEmpty(r, c)) {
                bool hasNeighbor = false;
                for (int dr = -distance; dr <= distance && !hasNeighbor; ++dr) {
                    for (int dc = -distance; dc <= distance; ++dc) {
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

std::vector<Move> getCandidateMoves(const Board& board, Player side) {
    return collectCandidateMoves(board, side, 2);
}

static int tacticalPriority(const Patterns::ThreatInfo& threat, bool attacking) {
    if (threat.createsFive) return attacking ? 10000000 : 9500000;
    if (threat.counts.liveFours > 0) return attacking ? 7000000 : 6800000;
    if (threat.counts.jumpFours > 0) return attacking ? 6500000 : 6300000;
    if (threat.counts.rushFours > 0) return attacking ? 6000000 : 5900000;
    if (threat.createsDoubleFour) return attacking ? 5600000 : 5500000;
    if (threat.createsFourThree) return attacking ? 5200000 : 5100000;
    if (threat.createsDoubleThree) return attacking ? 4800000 : 4700000;
    if (threat.counts.liveThrees > 0) return attacking ? 130000 : 150000;
    if (threat.counts.jumpLiveThrees > 0) return attacking ? 90000 : 110000;
    return 0;
}

static bool isForcedCandidate(const Board& board, const Move& move, Player side) {
    Player opp = opponentOf(side);
    return Patterns::isForcingMove(board, move.row, move.col, side) ||
           Patterns::isForcingMove(board, move.row, move.col, opp);
}

std::vector<ScoredMove> getScoredCandidateMoves(const Board& board, Player side,
                                                CandidateOptions options) {
    if (options.neighborDistance < 1) options.neighborDistance = 1;

    Player opp = opponentOf(side);
    auto candidates = collectCandidateMoves(board, side, options.neighborDistance);
    std::vector<ScoredMove> scored;
    scored.reserve(candidates.size());

    for (const auto& mv : candidates) {
        Patterns::ThreatInfo attack = Patterns::analyzeMoveThreat(board, mv.row, mv.col, side);
        Patterns::ThreatInfo defense = options.includeOpponentThreats
                                           ? Patterns::analyzeMoveThreat(board, mv.row, mv.col, opp)
                                           : Patterns::ThreatInfo{};

        ScoredMove sm;
        sm.move = mv;
        sm.attackScore = attack.score + tacticalPriority(attack, true);
        sm.defenseScore = (int)(defense.score * 1.2) + tacticalPriority(defense, false);
        sm.positionScore = Evaluation::getPositionValue(mv.row, mv.col, board.size());
        scored.push_back(sm);
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b) {
        if (a.totalScore() != b.totalScore()) return a.totalScore() > b.totalScore();
        if (a.attackScore != b.attackScore) return a.attackScore > b.attackScore;
        return a.positionScore > b.positionScore;
    });

    if (options.maxMoves > 0 && (int)scored.size() > options.maxMoves) {
        std::vector<ScoredMove> pruned;
        pruned.reserve(options.maxMoves);

        for (const auto& sm : scored) {
            bool keepForced =
                options.preserveForcingMoves && isForcedCandidate(board, sm.move, side);
            if ((int)pruned.size() < options.maxMoves || keepForced) {
                pruned.push_back(sm);
            }
        }

        scored = std::move(pruned);
    }

    return scored;
}

std::optional<Move> getFastRandomMove(const Board& board, Player side) {
    CandidateOptions options;
    options.maxMoves = 8;
    auto scored = getScoredCandidateMoves(board, side, options);
    if (scored.empty()) return std::nullopt;
    if (scored[0].totalScore() >= 1000000) return scored[0].move;

    double totalWeight = 0.0;
    std::vector<double> weights;
    weights.reserve(scored.size());

    for (const auto& sm : scored) {
        double weight = std::exp(std::min(10.0, sm.totalScore() / 50000.0));
        weights.push_back(weight);
        totalWeight += weight;
    }

    std::uniform_real_distribution<> dis(0.0, totalWeight);
    double sample = dis(rng);
    double cumulative = 0.0;
    for (size_t i = 0; i < scored.size(); ++i) {
        cumulative += weights[i];
        if (sample <= cumulative) return scored[i].move;
    }

    return scored[0].move;
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
    Player opp = opponentOf(side);
    auto candidates = collectCandidateMoves(board, side, 1);

    // First pass: can we win?
    for (const auto& mv : candidates) {
        if (wouldWin(board, mv.row, mv.col, side)) {
            return mv;
        }
    }

    // Second pass: must we block opponent's win?
    for (const auto& mv : candidates) {
        if (wouldWin(board, mv.row, mv.col, opp)) {
            return Move{mv.row, mv.col, side};
        }
    }

    return std::nullopt;
}

std::optional<Move> checkImmediateThreats(const Board& board, Player side) {
    Player opp = opponentOf(side);
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
    CandidateOptions options;
    options.maxMoves = 0;
    auto candidates = getScoredCandidateMoves(board, side, options);

    for (const auto& candidate : candidates) {
        const auto& mv = candidate.move;
        auto threats = Patterns::analyzeMoveThreat(board, mv.row, mv.col, side);

        // Check for winning combinations
        if (threats.counts.liveFours >= 1) return mv;
        if (threats.createsDoubleFour) return mv;
        if (threats.createsFourThree) return mv;
        if (threats.createsDoubleThree) return mv;
    }

    return std::nullopt;
}

// ========== Move Selection ==========

std::optional<Move> getBestHeuristicMove(const Board& board, Player side) {
    Player opp = opponentOf(side);

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
    auto scoredMoves = getScoredCandidateMoves(board, side);
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
    CandidateOptions options;
    options.maxMoves = 8;
    auto scored = getScoredCandidateMoves(board, side, options);

    if (scored.empty()) return std::nullopt;
    if (scored[0].totalScore() >= 1000000) return scored[0].move;

    int maxMoves = std::min(8, (int)scored.size());
    std::vector<double> weights;
    double totalWeight = 0.0;

    for (int i = 0; i < maxMoves; ++i) {
        double weight = std::exp(std::min(10.0, scored[i].totalScore() / 50000.0));
        weights.push_back(weight);
        totalWeight += weight;
    }

    if (totalWeight == 0) {
        return scored[0].move;
    }

    std::uniform_real_distribution<> dis(0.0, totalWeight);
    double sample = dis(rng);
    double cumulative = 0.0;

    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (sample <= cumulative) {
            return scored[i].move;
        }
    }

    return scored[0].move;
}

}  // namespace Heuristics
