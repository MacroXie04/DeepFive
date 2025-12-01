#ifndef PATTERNS_H
#define PATTERNS_H

#include <string>
#include <vector>

#include "../core/board.h"

namespace Patterns {

// Pattern types with scores (more granular classification)
enum class PatternType {
    FIVE = 0,         // five in a row
    LIVE_FOUR,        // live four
    RUSH_FOUR,        // rush four
    JUMP_FOUR,        // jump four
    LIVE_THREE,       // live three
    JUMP_LIVE_THREE,  // jump live three
    SLEEP_THREE,      // sleep three
    LIVE_TWO,         // live two
    JUMP_LIVE_TWO,    // jump live two
    SLEEP_TWO,        // sleep two
    NONE
};

// Pattern scores for evaluation (tuned values)
constexpr int SCORE_FIVE = 100000;
constexpr int SCORE_LIVE_FOUR = 50000;
constexpr int SCORE_RUSH_FOUR = 8000;
constexpr int SCORE_JUMP_FOUR = 7500;
constexpr int SCORE_LIVE_THREE = 4000;
constexpr int SCORE_JUMP_LIVE_THREE = 3500;
constexpr int SCORE_SLEEP_THREE = 800;
constexpr int SCORE_LIVE_TWO = 150;
constexpr int SCORE_JUMP_LIVE_TWO = 120;
constexpr int SCORE_SLEEP_TWO = 20;

// Bonus scores for combinations
constexpr int SCORE_DOUBLE_LIVE_THREE = 45000;
constexpr int SCORE_DOUBLE_RUSH_FOUR = 45000;
constexpr int SCORE_RUSH_FOUR_LIVE_THREE = 45000;

// Threat count structure for pattern analysis
struct ThreatCount {
    int liveFours = 0;
    int rushFours = 0;
    int jumpFours = 0;
    int liveThrees = 0;
    int jumpLiveThrees = 0;

    int totalFours() const { return liveFours + rushFours + jumpFours; }
    int totalThrees() const { return liveThrees + jumpLiveThrees; }
};

// Extract a line pattern as a string centered at (r,c)
// '.' = empty, 'X' = player, 'O' = opponent, '#' = out of bounds
std::string extractLinePattern(const Board& board, int r, int c, int dr, int dc, Player player,
                               int length = 9);

// Analyze a pattern string and return score
int analyzePatternString(const std::string& pattern);

// Count pattern score at a position in all directions
int countPatternScore(const Board& board, int row, int col, Player player);

// Count threats at a position (for combo detection)
ThreatCount countThreats(const Board& board, int row, int col, Player player);

// Fast heuristic score - lightweight version for rollout
int fastHeuristicScore(const Board& board, int r, int c, Player player);

}  // namespace Patterns

#endif
