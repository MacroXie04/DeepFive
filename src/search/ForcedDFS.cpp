#include "ForcedDFS.h"
#include "../bot/bot.h"
#include "../bot/heuristics.h"
#include <deque>
#include <memory>
#include <FL/Fl.H>

// Thread-local storage for DFS nodes
// Use std::deque to ensure pointers to elements remain valid upon insertion.
thread_local std::deque<std::unique_ptr<ForcedNode>> dfsNodePool;
thread_local int dfsNodesVisited = 0;

ForcedNode* DFS_Recursive(ForcedNode* node, Player targetSide, int maxDepth) {
    if ((++dfsNodesVisited & 0xFF) == 0) {
        Fl::check();
    }
    if (dfsNodesVisited > 200000) return nullptr;

    if (node->depth >= maxDepth) return nullptr;
    
    Player currentPlayer = node->toMove;
    Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
    
    std::vector<Move> moves = Heuristics::getCandidateMoves(node->board, currentPlayer);
    
    for (const auto& mv : moves) {
        Board newBoard = node->board;
        newBoard.placeStone(mv.row, mv.col, mv.player);
        
        if (newBoard.checkWinner() == targetSide) {
            dfsNodePool.push_back(std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
            return dfsNodePool.back().get();
        }
        
        Player winner = newBoard.checkWinner();
        if (winner != Player::NoPlayer && winner != targetSide) {
            continue; // Loss branch
        }
        
        dfsNodePool.push_back(std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
        ForcedNode* child = dfsNodePool.back().get();
        
        ForcedNode* result = DFS_Recursive(child, targetSide, maxDepth);
        if (result != nullptr) return result;
    }
    
    return nullptr;
}

ForcedNode* DFS_FindWin(const Board& board, Player side, int maxDepth) {
    dfsNodePool.clear();
    dfsNodesVisited = 0;
    
    // Root
    dfsNodePool.push_back(std::make_unique<ForcedNode>(board, side, Move{-1,-1,Player::NoPlayer}, nullptr, 0));
    ForcedNode* root = dfsNodePool.back().get();
    
    return DFS_Recursive(root, side, maxDepth);
}
