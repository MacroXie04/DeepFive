#ifndef HEURISTICS_H
#define HEURISTICS_H

#include <optional>
#include <vector>

#include "../core/board.h"

namespace Heuristics {

// Returns plausible moves to consider for MCTS or other searches
std::vector<Move> getCandidateMoves(const Board& board, Player side);

// Returns a random move near existing stones (fast rollout policy)
std::optional<Move> getFastRandomMove(const Board& board, Player side);

// Check for immediate win (5) or forced block (4)
std::optional<Move> checkImmediateThreats(const Board& board, Player side);

}  // namespace Heuristics

#endif
