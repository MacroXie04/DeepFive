#include "patterns.h"

#include <algorithm>
#include <cmath>

namespace Patterns {

// ============== String-Based Pattern Recognition ==============

std::string extractLinePattern(const Board& board, int r, int c, int dr, int dc, Player player,
                               int length) {
    std::string pattern;
    pattern.reserve(length);

    int halfLen = length / 2;
    int startR = r - halfLen * dr;
    int startC = c - halfLen * dc;

    for (int i = 0; i < length; ++i) {
        int nr = startR + i * dr;
        int nc = startC + i * dc;

        if (!board.isInside(nr, nc)) {
            pattern += '#';
        } else if (board.isEmpty(nr, nc)) {
            pattern += '.';
        } else if (board.at(nr, nc) == player) {
            pattern += 'X';
        } else {
            pattern += 'O';
        }
    }

    return pattern;
}

// Check if pattern matches any of the given templates
static bool matchesAny(const std::string& pattern, const std::vector<std::string>& templates) {
    for (const auto& tmpl : templates) {
        if (pattern.find(tmpl) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static int patternRank(PatternType type) {
    switch (type) {
        case PatternType::FIVE:
            return 10;
        case PatternType::LIVE_FOUR:
            return 9;
        case PatternType::JUMP_FOUR:
            return 8;
        case PatternType::RUSH_FOUR:
            return 7;
        case PatternType::LIVE_THREE:
            return 6;
        case PatternType::JUMP_LIVE_THREE:
            return 5;
        case PatternType::SLEEP_THREE:
            return 4;
        case PatternType::LIVE_TWO:
            return 3;
        case PatternType::JUMP_LIVE_TWO:
            return 2;
        case PatternType::SLEEP_TWO:
            return 1;
        case PatternType::NONE:
            return 0;
    }
    return 0;
}

static void setHighest(PatternType& highest, PatternType candidate) {
    if (patternRank(candidate) > patternRank(highest)) {
        highest = candidate;
    }
}

static bool hasFourWindow(const std::string& pattern) {
    if (pattern.size() < 5) return false;
    for (size_t i = 0; i + 5 <= pattern.size(); ++i) {
        int xCount = 0;
        int dotCount = 0;
        bool blocked = false;
        for (size_t j = i; j < i + 5; ++j) {
            if (pattern[j] == 'X')
                xCount++;
            else if (pattern[j] == '.')
                dotCount++;
            else
                blocked = true;
        }
        if (!blocked && xCount == 4 && dotCount == 1) return true;
    }
    return false;
}

static ThreatInfo analyzePatternLine(const std::string& pattern) {
    ThreatInfo info;

    static const std::vector<std::string> liveFourPatterns = {".XXXX."};
    static const std::vector<std::string> jumpFourPatterns = {".XXX.X.", ".XX.XX.", ".X.XXX."};
    static const std::vector<std::string> rushFourPatterns = {
        "OXXXX.", ".XXXXO", "#XXXX.", ".XXXX#", "OXXX.X", "XXX.XO", "#XXX.X", "XXX.X#",
        "OXX.XX", "XX.XXO", "#XX.XX", "XX.XX#", "OX.XXX", "X.XXXO", "#X.XXX", "X.XXX#"};
    static const std::vector<std::string> liveThreePatterns = {".XXX..", "..XXX.", ".XXX."};
    static const std::vector<std::string> jumpLiveThreePatterns = {".X.XX.", ".XX.X.", ".X.X.",
                                                                   ".X..XX.", ".XX..X."};
    static const std::vector<std::string> sleepThreePatterns = {
        "OXXX..", "..XXXO", "#XXX..", "..XXX#", "OX.XX.", ".XX.XO",
        "OXX.X.", ".X.XXO", "OXXX.",  ".XXXO",  "#XXX.",  ".XXX#"};
    static const std::vector<std::string> liveTwoPatterns = {".XX..", "..XX.", ".XX."};
    static const std::vector<std::string> jumpLiveTwoPatterns = {".X.X.", ".X..X.", "..X.X."};

    if (pattern.find("XXXXX") != std::string::npos) {
        info.createsFive = true;
        info.score += SCORE_FIVE;
        setHighest(info.highestPattern, PatternType::FIVE);
        return info;
    }

    if (matchesAny(pattern, liveFourPatterns)) {
        info.counts.liveFours++;
        info.score += SCORE_LIVE_FOUR;
        setHighest(info.highestPattern, PatternType::LIVE_FOUR);
    } else if (matchesAny(pattern, jumpFourPatterns)) {
        info.counts.jumpFours++;
        info.score += SCORE_JUMP_FOUR;
        setHighest(info.highestPattern, PatternType::JUMP_FOUR);
    } else if (matchesAny(pattern, rushFourPatterns) || hasFourWindow(pattern)) {
        info.counts.rushFours++;
        info.score += SCORE_RUSH_FOUR;
        setHighest(info.highestPattern, PatternType::RUSH_FOUR);
    } else if (matchesAny(pattern, liveThreePatterns)) {
        info.counts.liveThrees++;
        info.score += SCORE_LIVE_THREE;
        setHighest(info.highestPattern, PatternType::LIVE_THREE);
    } else if (matchesAny(pattern, jumpLiveThreePatterns)) {
        info.counts.jumpLiveThrees++;
        info.score += SCORE_JUMP_LIVE_THREE;
        setHighest(info.highestPattern, PatternType::JUMP_LIVE_THREE);
    } else if (matchesAny(pattern, sleepThreePatterns)) {
        info.counts.sleepThrees++;
        info.score += SCORE_SLEEP_THREE;
        setHighest(info.highestPattern, PatternType::SLEEP_THREE);
    } else if (matchesAny(pattern, liveTwoPatterns)) {
        info.counts.liveTwos++;
        info.score += SCORE_LIVE_TWO;
        setHighest(info.highestPattern, PatternType::LIVE_TWO);
    } else if (matchesAny(pattern, jumpLiveTwoPatterns)) {
        info.counts.jumpLiveTwos++;
        info.score += SCORE_JUMP_LIVE_TWO;
        setHighest(info.highestPattern, PatternType::JUMP_LIVE_TWO);
    }

    return info;
}

int analyzePatternString(const std::string& pattern) {
    return analyzePatternLine(pattern).score;
}

// ============== Pattern Score Counting ==============

int countPatternScore(const Board& board, int row, int col, Player player) {
    return analyzeMoveThreat(board, row, col, player).score;
}

ThreatCount countThreats(const Board& board, int row, int col, Player player) {
    return analyzeMoveThreat(board, row, col, player).counts;
}

ThreatInfo analyzeMoveThreat(const Board& board, int row, int col, Player player) {
    ThreatInfo info;
    if (!board.isEmpty(row, col) || player == Player::NoPlayer) return info;

    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    Board tempBoard = board;
    tempBoard.placeStone(row, col, player);

    for (auto& dir : directions) {
        std::string pattern = extractLinePattern(tempBoard, row, col, dir[0], dir[1], player, 9);
        ThreatInfo line = analyzePatternLine(pattern);
        info.createsFive = info.createsFive || line.createsFive;
        info.counts.liveFours += line.counts.liveFours;
        info.counts.rushFours += line.counts.rushFours;
        info.counts.jumpFours += line.counts.jumpFours;
        info.counts.liveThrees += line.counts.liveThrees;
        info.counts.jumpLiveThrees += line.counts.jumpLiveThrees;
        info.counts.sleepThrees += line.counts.sleepThrees;
        info.counts.liveTwos += line.counts.liveTwos;
        info.counts.jumpLiveTwos += line.counts.jumpLiveTwos;
        info.score += line.score;
        setHighest(info.highestPattern, line.highestPattern);
    }

    info.createsDoubleFour = info.counts.totalFours() >= 2;
    info.createsFourThree = info.counts.totalFours() >= 1 && info.counts.totalThrees() >= 1;
    info.createsDoubleThree = info.counts.totalThrees() >= 2;

    if (info.createsDoubleFour) info.score += SCORE_DOUBLE_RUSH_FOUR;
    if (info.createsFourThree) info.score += SCORE_RUSH_FOUR_LIVE_THREE;
    if (info.createsDoubleThree) info.score += SCORE_DOUBLE_LIVE_THREE;

    info.forcedWin = isForcingMove(info);
    return info;
}

bool isForcingMove(const ThreatInfo& threat) {
    return threat.createsFive || threat.counts.totalFours() > 0 || threat.createsFourThree ||
           threat.createsDoubleThree;
}

bool isForcingMove(const Board& board, int row, int col, Player player) {
    return isForcingMove(analyzeMoveThreat(board, row, col, player));
}

// ============== Fast Heuristic Score ==============

// Fast line counting helper
static int fastCountLine(const Board& board, int r, int c, int dr, int dc, Player player) {
    int count = 0;
    int nr = r + dr, nc = c + dc;
    while (board.isInside(nr, nc) && board.at(nr, nc) == player) {
        count++;
        nr += dr;
        nc += dc;
    }
    return count;
}

int fastHeuristicScore(const Board& board, int r, int c, Player player) {
    if (!board.isEmpty(r, c)) return 0;

    ThreatInfo threat = analyzeMoveThreat(board, r, c, player);
    int score = threat.score;
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    for (auto& dir : directions) {
        int fwd = fastCountLine(board, r, c, dir[0], dir[1], player);
        int bwd = fastCountLine(board, r, c, -dir[0], -dir[1], player);
        int total = 1 + fwd + bwd;

        // Check if ends are open
        int fwdR = r + (fwd + 1) * dir[0], fwdC = c + (fwd + 1) * dir[1];
        int bwdR = r - (bwd + 1) * dir[0], bwdC = c - (bwd + 1) * dir[1];
        bool openFwd = board.isInside(fwdR, fwdC) && board.isEmpty(fwdR, fwdC);
        bool openBwd = board.isInside(bwdR, bwdC) && board.isEmpty(bwdR, bwdC);
        int openEnds = (openFwd ? 1 : 0) + (openBwd ? 1 : 0);

        // Score based on line length and openness
        if (total >= 5) {
            score += 100000;
        } else if (total == 4) {
            score += (openEnds == 2) ? 50000 : (openEnds == 1 ? 8000 : 0);
        } else if (total == 3) {
            score += (openEnds == 2) ? 4000 : (openEnds == 1 ? 800 : 0);
        } else if (total == 2) {
            score += (openEnds == 2) ? 150 : (openEnds == 1 ? 20 : 0);
        }
    }

    return score;
}

}  // namespace Patterns
