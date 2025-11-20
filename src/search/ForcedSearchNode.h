#ifndef FORCED_SEARCH_NODE_H
#define FORCED_SEARCH_NODE_H

#include "../core/board.h"

struct ForcedNode {
    Board board;
    Player toMove;
    Move move;            // move applied from parent -> this node
    ForcedNode* parent;   // for path reconstruction
    int depth;

    ForcedNode(const Board& b, Player p, Move m, ForcedNode* par, int d)
        : board(b), toMove(p), move(m), parent(par), depth(d) {}
};

#endif
