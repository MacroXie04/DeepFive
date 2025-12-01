#include "heuristics.h"

#include <algorithm>
#include <random>
#include <thread>

namespace Heuristics {

// Thread-local random generator
static thread_local std::mt19937 rng(std::random_device{}());

std::vector<Move> getCandidateMoves(const Board& board, Player side) {
    std::vector<Move> moves;
    moves.reserve(50);
    int size = board.size();

    // Optimization: Only scan relevant area
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

    // Expand bounds by 2
    minR = std::max(0, minR - 2);
    maxR = std::min(size - 1, maxR + 2);
    minC = std::max(0, minC - 2);
    maxC = std::min(size - 1, maxC + 2);

    for (int r = minR; r <= maxR; ++r) {
        for (int c = minC; c <= maxC; ++c) {
            if (board.isEmpty(r, c)) {
                // Check if it has neighbor within 2
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

    // Try neighbors of random stones
    for (const auto& s : stones) {
        int r = s.first;
        int c = s.second;

        // Randomize neighbor order
        int drs[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        int dcs[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int indices[] = {0, 1, 2, 3, 4, 5, 6, 7};

        // Partial shuffle of indices
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

std::optional<Move> checkImmediateThreats(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    auto candidates = getCandidateMoves(board, side);

    // 1. Can we win directly?
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, side);
        if (temp.checkWinner() == side) {
            return move;
        }
    }

    // 2. Must we block opponent win (Connect 4)?
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, opp);
        if (temp.checkWinner() == opp) {
            return Move{move.row, move.col, side};  // Block it!
        }
    }

    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, opp);

        int r = move.row;
        int c = move.col;
        int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

        for (auto& dir : directions) {
            int count = 1;
            // Check forward
            int i = 1;
            while (temp.isInside(r + i * dir[0], c + i * dir[1]) &&
                   temp.at(r + i * dir[0], c + i * dir[1]) == opp) {
                count++;
                i++;
            }
            bool openFront = temp.isInside(r + i * dir[0], c + i * dir[1]) &&
                             temp.isEmpty(r + i * dir[0], c + i * dir[1]);

            // Check backward
            int j = 1;
            while (temp.isInside(r - j * dir[0], c - j * dir[1]) &&
                   temp.at(r - j * dir[0], c - j * dir[1]) == opp) {
                count++;
                j++;
            }
            bool openBack = temp.isInside(r - j * dir[0], c - j * dir[1]) &&
                            temp.isEmpty(r - j * dir[0], c - j * dir[1]);

            if (count == 4 && openFront && openBack) {
                return Move{move.row, move.col, side};
            }
            if (count == 4 && (openFront || openBack)) {
                return Move{move.row, move.col, side};
            }
        }
    }

    return std::nullopt;
}

// Helper: count consecutive stones in a direction from (r,c), not including (r,c)
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

// Helper: check if placing at (r,c) creates a line of 5+ for player
static bool wouldWin(const Board& board, int r, int c, Player player) {
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (auto& dir : directions) {
        int count = 1;  // The stone we're placing
        count += countDirection(board, r, c, dir[0], dir[1], player);
        count += countDirection(board, r, c, -dir[0], -dir[1], player);
        if (count >= 5) return true;
    }
    return false;
}

std::optional<Move> getFastThreatMove(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    int size = board.size();
    
    // Find bounds of existing stones
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
    
    if (maxR < 0) return std::nullopt;  // Empty board
    
    // Expand by 1
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

// ============== Pattern Recognition and Scoring ==============

// Analyze a line pattern in one direction
// Returns: (consecutive count, open ends count, has gap)
struct LineInfo {
    int count;      // Consecutive stones (including the hypothetical one at r,c)
    int openEnds;   // 0, 1, or 2 open ends
    bool hasGap;    // Has a gap that can be filled
};

static LineInfo analyzeLineInDirection(const Board& board, int r, int c, 
                                       int dr, int dc, Player player) {
    LineInfo info = {1, 0, false};  // Start with the stone at (r,c)
    
    // Count forward
    int countFwd = 0;
    int gapFwd = 0;
    int nr = r + dr, nc = c + dc;
    while (board.isInside(nr, nc)) {
        if (board.at(nr, nc) == player) {
            countFwd++;
        } else if (board.isEmpty(nr, nc) && gapFwd == 0) {
            // Check if there's a stone after the gap
            int nnr = nr + dr, nnc = nc + dc;
            if (board.isInside(nnr, nnc) && board.at(nnr, nnc) == player) {
                gapFwd = 1;
                countFwd++;
                nr = nnr;
                nc = nnc;
                continue;
            } else {
                info.openEnds++;
                break;
            }
        } else {
            if (board.isEmpty(nr, nc)) info.openEnds++;
            break;
        }
        nr += dr;
        nc += dc;
    }
    
    // Count backward
    int countBwd = 0;
    int gapBwd = 0;
    nr = r - dr;
    nc = c - dc;
    while (board.isInside(nr, nc)) {
        if (board.at(nr, nc) == player) {
            countBwd++;
        } else if (board.isEmpty(nr, nc) && gapBwd == 0) {
            int nnr = nr - dr, nnc = nc - dc;
            if (board.isInside(nnr, nnc) && board.at(nnr, nnc) == player) {
                gapBwd = 1;
                countBwd++;
                nr = nnr;
                nc = nnc;
                continue;
            } else {
                info.openEnds++;
                break;
            }
        } else {
            if (board.isEmpty(nr, nc)) info.openEnds++;
            break;
        }
        nr -= dr;
        nc -= dc;
    }
    
    info.count = 1 + countFwd + countBwd;
    info.hasGap = (gapFwd > 0 || gapBwd > 0);
    
    return info;
}

// Get the pattern type from line info
static Pattern getPatternFromLine(const LineInfo& info) {
    if (info.count >= 5) return Pattern::FIVE;
    if (info.count == 4) {
        if (info.openEnds == 2) return Pattern::LIVE_FOUR;
        if (info.openEnds == 1) return Pattern::RUSH_FOUR;
    }
    if (info.count == 3) {
        if (info.openEnds == 2) return Pattern::LIVE_THREE;
        if (info.openEnds == 1) return Pattern::SLEEP_THREE;
    }
    if (info.count == 2) {
        if (info.openEnds == 2) return Pattern::LIVE_TWO;
        if (info.openEnds == 1) return Pattern::SLEEP_TWO;
    }
    return Pattern::NONE;
}

// Get score for a pattern
static int getPatternScore(Pattern p) {
    switch (p) {
        case Pattern::FIVE: return SCORE_FIVE;
        case Pattern::LIVE_FOUR: return SCORE_LIVE_FOUR;
        case Pattern::RUSH_FOUR: return SCORE_RUSH_FOUR;
        case Pattern::LIVE_THREE: return SCORE_LIVE_THREE;
        case Pattern::SLEEP_THREE: return SCORE_SLEEP_THREE;
        case Pattern::LIVE_TWO: return SCORE_LIVE_TWO;
        case Pattern::SLEEP_TWO: return SCORE_SLEEP_TWO;
        default: return 0;
    }
}

int countPatternScore(const Board& board, int row, int col, Player player) {
    if (!board.isEmpty(row, col)) return 0;
    
    int totalScore = 0;
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    
    int liveThrees = 0;
    int rushFours = 0;
    
    for (auto& dir : directions) {
        LineInfo info = analyzeLineInDirection(board, row, col, dir[0], dir[1], player);
        Pattern p = getPatternFromLine(info);
        totalScore += getPatternScore(p);
        
        if (p == Pattern::LIVE_THREE) liveThrees++;
        if (p == Pattern::RUSH_FOUR) rushFours++;
    }
    
    // Bonus for double threats
    if (liveThrees >= 2) totalScore += SCORE_LIVE_FOUR;  // 双活三 ≈ 活四
    if (rushFours >= 2) totalScore += SCORE_LIVE_FOUR;   // 双冲四 ≈ 活四
    if (rushFours >= 1 && liveThrees >= 1) totalScore += SCORE_LIVE_FOUR;  // 冲四活三 ≈ 活四
    
    return totalScore;
}

int evaluatePosition(const Board& board, int row, int col, Player player) {
    return countPatternScore(board, row, col, player);
}

std::vector<ScoredMove> getScoredMoves(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    auto candidates = getCandidateMoves(board, side);
    
    std::vector<ScoredMove> scored;
    scored.reserve(candidates.size());
    
    for (const auto& mv : candidates) {
        ScoredMove sm;
        sm.move = mv;
        sm.attackScore = countPatternScore(board, mv.row, mv.col, side);
        sm.defenseScore = countPatternScore(board, mv.row, mv.col, opp);
        scored.push_back(sm);
    }
    
    // Sort by total score (descending)
    std::sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.totalScore() > b.totalScore();
    });
    
    return scored;
}

std::optional<Move> findDoubleThreat(const Board& board, Player side) {
    auto candidates = getCandidateMoves(board, side);
    
    for (const auto& mv : candidates) {
        int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
        
        int liveThrees = 0;
        int rushFours = 0;
        int liveFours = 0;
        
        for (auto& dir : directions) {
            LineInfo info = analyzeLineInDirection(board, mv.row, mv.col, dir[0], dir[1], side);
            Pattern p = getPatternFromLine(info);
            
            if (p == Pattern::LIVE_FOUR) liveFours++;
            if (p == Pattern::RUSH_FOUR) rushFours++;
            if (p == Pattern::LIVE_THREE) liveThrees++;
        }
        
        // Check for double threat patterns
        if (liveFours >= 1) return mv;  // 活四必胜
        if (liveThrees >= 2) return mv;  // 双活三
        if (rushFours >= 2) return mv;   // 双冲四
        if (rushFours >= 1 && liveThrees >= 1) return mv;  // 冲四活三
    }
    
    return std::nullopt;
}

std::optional<Move> getBestHeuristicMove(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    
    // 1. Can we win directly (Five)?
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
    auto scoredMoves = getScoredMoves(board, side);
    if (!scoredMoves.empty()) {
        // Add some randomness among top moves to avoid predictability
        int topCount = std::min(3, (int)scoredMoves.size());
        int bestScore = scoredMoves[0].totalScore();
        
        // Only consider moves within 80% of best score
        int threshold = (int)(bestScore * 0.8);
        topCount = 0;
        for (const auto& sm : scoredMoves) {
            if (sm.totalScore() >= threshold) topCount++;
            else break;
        }
        
        if (topCount > 1) {
            std::uniform_int_distribution<> dis(0, topCount - 1);
            return scoredMoves[dis(rng)].move;
        }
        
        return scoredMoves[0].move;
    }
    
    return std::nullopt;
}

}  // namespace Heuristics
