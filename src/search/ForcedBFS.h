#ifndef FORCED_BFS_H
#define FORCED_BFS_H

#include "ForcedSearchNode.h"

// Finds a forced win for 'side' within maxDepth.
// Returns a pointer to the leaf node of the winning path, or nullptr if no win found.
ForcedNode* BFS_FindWin(const Board& board, Player side, int maxDepth);

// Finds if the opponent has a forced win (threat) that we need to block.
// Returns the leaf node of the opponent's winning path.
// The caller can reconstruct the path and take the first move (opponent's winning move) to block
// it.
ForcedNode* BFS_FindLose(const Board& board, Player side, int maxDepth);

#endif
