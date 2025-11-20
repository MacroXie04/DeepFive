#include "ForcedDFS.h"
#include "../ai/bot.h"
#include <vector>
#include <memory>

// Thread-local storage for DFS nodes
thread_local std::vector<std::unique_ptr<ForcedNode>> dfsNodePool;

ForcedNode* DFS_Recursive(ForcedNode* node, Player targetSide, int maxDepth) {
    if (node->depth >= maxDepth) return nullptr;
    
    Player currentPlayer = node->toMove;
    Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
    
    std::vector<Move> moves = GomokuBot::getCandidateMoves(node->board, currentPlayer);
    
    for (const auto& mv : moves) {
        Board newBoard = node->board;
        newBoard.placeStone(mv.row, mv.col, mv.player);
        
        if (newBoard.checkWinner() == targetSide) {
            dfsNodePool.push_back(std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
            return dfsNodePool.back().get();
        }
        
        Player winner = newBoard.checkWinner();
        if (winner != Player::None && winner != targetSide) {
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
    
    // Root
    dfsNodePool.push_back(std::make_unique<ForcedNode>(board, side, Move{-1,-1,Player::None}, nullptr, 0));
    ForcedNode* root = dfsNodePool.back().get();
    
    return DFS_Recursive(root, side, maxDepth);
}
