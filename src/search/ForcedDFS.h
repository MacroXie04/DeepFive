#ifndef FORCED_DFS_H
#define FORCED_DFS_H

#include "ForcedSearchNode.h"

// Finds a forced win for 'side' within maxDepth using DFS.
// Returns a pointer to the leaf node of the winning path, or nullptr if no win found.
ForcedNode* DFS_FindWin(const Board& board, Player side, int maxDepth);

#endif
