#ifndef FORCED_DFS_H
#define FORCED_DFS_H

#include "ForcedSearchNode.h"

// Finds a forced win for 'side' within maxDepth using DFS.
// Returns a pointer to the leaf node of the winning path, or nullptr if no win found.
ForcedNode* DFS_FindWin(const Board& board, Player side, int maxDepth);

// Victory by Continuous Fours (VCF) solver.
// Only considers moves that create a "Four" (forcing) or block opponent's "Four".
// Returns a pointer to the leaf node of the winning path, or nullptr if no VCF found.
ForcedNode* VCF_Solve(const Board& board, Player side, int maxDepth = 15);

// Victory by Continuous Threes (VCT) solver.
// Considers moves that create "Live Three" or "Rush Four" (forcing sequences).
// More aggressive than VCF, finds wins through continuous threats.
// Returns a pointer to the leaf node of the winning path, or nullptr if no VCT found.
ForcedNode* VCT_Solve(const Board& board, Player side, int maxDepth = 12);

#endif
