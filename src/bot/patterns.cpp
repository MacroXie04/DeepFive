#include "patterns.h"

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

int analyzePatternString(const std::string& pattern) {
    int score = 0;

    // Five patterns (already won)
    static const std::vector<std::string> fivePatterns = {"XXXXX"};

    // Live four patterns (must win) - _XXXX_
    static const std::vector<std::string> liveFourPatterns = {".XXXX."};

    // Rush four patterns (rush four) - opponent blocked one end
    static const std::vector<std::string> rushFourPatterns = {"OXXXX.", ".XXXXO", "#XXXX.",
                                                              ".XXXX#"};

    // Jump four patterns (jump four) - X_XXX, XX_XX, XXX_X
    static const std::vector<std::string> jumpFourPatterns = {"X.XXX", "XX.XX", "XXX.X"};

    // Live three patterns (live three) - _XXX_ with space to grow
    static const std::vector<std::string> liveThreePatterns = {".XXX..", "..XXX.", ".XXX."};

    // Jump live three patterns (jump live three) - _X_XX_, _XX_X_
    static const std::vector<std::string> jumpLiveThreePatterns = {".X.XX.", ".XX.X.", ".X.XX",
                                                                   "XX.X.", "X.XX."};

    // Sleep three patterns (sleep three)
    static const std::vector<std::string> sleepThreePatterns = {
        "OXXX..", "..XXXO", "#XXX..", "..XXX#", "OX.XX.", ".XX.XO",
        "OXX.X.", ".X.XXO", "OXXX.",  ".XXXO",  "#XXX.",  ".XXX#"};

    // Live two patterns (live two)
    static const std::vector<std::string> liveTwoPatterns = {".XX..", "..XX.", ".XX."};

    // Jump live two patterns (jump live two)
    static const std::vector<std::string> jumpLiveTwoPatterns = {".X.X.", ".X..X.", "..X.X."};

    // Check patterns in order of importance
    if (matchesAny(pattern, fivePatterns)) {
        score += SCORE_FIVE;
    }
    if (matchesAny(pattern, liveFourPatterns)) {
        score += SCORE_LIVE_FOUR;
    }
    if (matchesAny(pattern, rushFourPatterns)) {
        score += SCORE_RUSH_FOUR;
    }
    if (matchesAny(pattern, jumpFourPatterns)) {
        score += SCORE_JUMP_FOUR;
    }
    if (matchesAny(pattern, liveThreePatterns)) {
        score += SCORE_LIVE_THREE;
    }
    if (matchesAny(pattern, jumpLiveThreePatterns)) {
        score += SCORE_JUMP_LIVE_THREE;
    }
    if (matchesAny(pattern, sleepThreePatterns)) {
        score += SCORE_SLEEP_THREE;
    }
    if (matchesAny(pattern, liveTwoPatterns)) {
        score += SCORE_LIVE_TWO;
    }
    if (matchesAny(pattern, jumpLiveTwoPatterns)) {
        score += SCORE_JUMP_LIVE_TWO;
    }

    return score;
}

// ============== Pattern Score Counting ==============

int countPatternScore(const Board& board, int row, int col, Player player) {
    if (!board.isEmpty(row, col)) return 0;

    int totalScore = 0;
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    // Create a temporary board with the hypothetical stone placed
    Board tempBoard = board;
    tempBoard.placeStone(row, col, player);

    ThreatCount threats;

    // Pattern templates for threat detection
    static const std::vector<std::string> liveFourPatterns = {".XXXX."};
    static const std::vector<std::string> rushFourPatterns = {"OXXXX.", ".XXXXO", "#XXXX.",
                                                              ".XXXX#"};
    static const std::vector<std::string> jumpFourPatterns = {"X.XXX", "XX.XX", "XXX.X"};
    static const std::vector<std::string> liveThreePatterns = {".XXX..", "..XXX."};
    static const std::vector<std::string> jumpLiveThreePatterns = {".X.XX.", ".XX.X."};

    for (auto& dir : directions) {
        std::string pattern = extractLinePattern(tempBoard, row, col, dir[0], dir[1], player, 9);
        totalScore += analyzePatternString(pattern);

        // Track threats for combo detection
        if (matchesAny(pattern, liveFourPatterns))
            threats.liveFours++;
        else if (matchesAny(pattern, rushFourPatterns))
            threats.rushFours++;
        else if (matchesAny(pattern, jumpFourPatterns))
            threats.jumpFours++;
        else if (matchesAny(pattern, liveThreePatterns))
            threats.liveThrees++;
        else if (matchesAny(pattern, jumpLiveThreePatterns))
            threats.jumpLiveThrees++;
    }

    // Bonus for double threats (winning combinations)
    if (threats.liveFours >= 1) {
        // Live four is already a winning position
    } else if (threats.totalFours() >= 2) {
        totalScore += SCORE_DOUBLE_RUSH_FOUR;
    } else if (threats.totalFours() >= 1 && threats.totalThrees() >= 1) {
        totalScore += SCORE_RUSH_FOUR_LIVE_THREE;
    } else if (threats.totalThrees() >= 2) {
        totalScore += SCORE_DOUBLE_LIVE_THREE;
    }

    return totalScore;
}

ThreatCount countThreats(const Board& board, int row, int col, Player player) {
    ThreatCount threats;
    if (!board.isEmpty(row, col)) return threats;

    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    Board tempBoard = board;
    tempBoard.placeStone(row, col, player);

    static const std::vector<std::string> liveFourPatterns = {".XXXX."};
    static const std::vector<std::string> rushFourPatterns = {"OXXXX.", ".XXXXO", "#XXXX.",
                                                              ".XXXX#"};
    static const std::vector<std::string> jumpFourPatterns = {"X.XXX", "XX.XX", "XXX.X"};
    static const std::vector<std::string> liveThreePatterns = {".XXX..", "..XXX."};
    static const std::vector<std::string> jumpLiveThreePatterns = {".X.XX.", ".XX.X."};

    for (auto& dir : directions) {
        std::string pattern = extractLinePattern(tempBoard, row, col, dir[0], dir[1], player, 9);

        if (matchesAny(pattern, liveFourPatterns))
            threats.liveFours++;
        else if (matchesAny(pattern, rushFourPatterns))
            threats.rushFours++;
        else if (matchesAny(pattern, jumpFourPatterns))
            threats.jumpFours++;
        else if (matchesAny(pattern, liveThreePatterns))
            threats.liveThrees++;
        else if (matchesAny(pattern, jumpLiveThreePatterns))
            threats.jumpLiveThrees++;
    }

    return threats;
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

    int score = 0;
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
