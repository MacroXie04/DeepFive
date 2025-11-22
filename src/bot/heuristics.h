#ifndef HEURISTICS_H
#define HEURISTICS_H

#include "../core/board.h"
#include <vector>
#include <optional>

namespace Heuristics {

    // Returns plausible moves to consider for MCTS or other searches
    std::vector<Move> getCandidateMoves(const Board& board, Player side);

    // Returns a random move near existing stones (fast rollout policy)
    std::optional<Move> getFastRandomMove(const Board& board, Player side);

    // Check for immediate win (5) or forced block (4)
    std::optional<Move> checkImmediateThreats(const Board& board, Player side);

}

#endif
