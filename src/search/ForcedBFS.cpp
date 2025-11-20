#include "ForcedBFS.h"
#include "../ai/bot.h"
#include <queue>
#include <deque>
#include <memory>
#include <FL/Fl.H>

// Thread-local storage to manage node memory without leaks.
// Cleared at the start of each search.
// Use std::deque to ensure pointers to elements remain valid upon insertion.
thread_local std::deque<std::unique_ptr<ForcedNode>> bfsNodePool;

ForcedNode* BFS_FindWin(const Board& board, Player side, int maxDepth) {
    bfsNodePool.clear();
    
    std::queue<ForcedNode*> q;
    
    bfsNodePool.push_back(std::make_unique<ForcedNode>(board, side, Move{-1,-1,Player::None}, nullptr, 0));
    ForcedNode* root = bfsNodePool.back().get();
    q.push(root);
    
    int nodesVisited = 0;

    while (!q.empty()) {
        // Keep UI responsive
        if ((++nodesVisited & 0xFF) == 0) {
             Fl::check();
        }
        if (nodesVisited > 200000) break; // Safety cutoff

        ForcedNode* node = q.front();
        q.pop();
        
        if (node->depth >= maxDepth) continue;
        
        Player currentPlayer = node->toMove;
        Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
        
        // Generate moves
        std::vector<Move> moves = GomokuBot::getCandidateMoves(node->board, currentPlayer);
        
        for (const auto& mv : moves) {
            // Create new board
            Board newBoard = node->board;
            newBoard.placeStone(mv.row, mv.col, mv.player);
            
            // Check win
            if (newBoard.checkWinner() == side) {
                // Found a win for 'side'
                bfsNodePool.push_back(std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
                return bfsNodePool.back().get();
            }
            
            // If opponent wins, this path is bad for me (if I am playing).
            // But BFS just finds *a* path.
            // If I am 'side', and I play M1, and opponent plays M2 and wins...
            // Then M1 is bad.
            // But this BFS is just looking for a state where 'side' has won.
            // It doesn't verify safety.
            
            // However, for "Forced Win", we usually want to ensure we win.
            // But with the given instructions, I will implement the BFS expansion.
            
            // Pruning: If opponent wins immediately in this branch, we shouldn't continue this branch?
            // If `newBoard.checkWinner() == opponent`, then we lost.
            // So don't add to queue.
            Player winner = newBoard.checkWinner();
            if (winner != Player::None && winner != side) {
                continue; // Dead end (loss)
            }
            
            // Add to queue
            bfsNodePool.push_back(std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
            q.push(bfsNodePool.back().get());
        }
    }
    
    return nullptr;
}

ForcedNode* BFS_FindLose(const Board& board, Player side, int maxDepth) {
    // Check if opponent has a winning path starting from NOW.
    // Opponent is 'other'.
    Player opponent = (side == Player::Black) ? Player::White : Player::Black;
    
    // We use BFS_FindWin for the opponent.
    // But BFS_FindWin assumes it's the player's turn.
    // Here it is 'side's turn.
    // So we can't just call BFS_FindWin(board, opponent).
    // We need to see if opponent has a win *threat*.
    // Actually, if it's my turn, opponent can't win unless I let them.
    // BFS_FindLose is likely intended to be used as:
    // "If I don't defend, opponent wins".
    // So we check: If I pass, does opponent win?
    // But I can't pass.
    
    // Correct logic for "Forced Defense":
    // Find a move M for ME such that opponent CANNOT win.
    // But the integration code expects `BFS_FindLose` to return a node, and we play `path.front()`.
    // This implies `BFS_FindLose` returns the *threat* sequence.
    // And we block the first move of the threat.
    
    // So, we simulate a "Pass" for me (or just check opponent's win from current board as if it was their turn).
    // If `BFS_FindWin(board, opponent)` finds a win, it means there is a sequence of moves
    // starting with Opponent that leads to Opponent win.
    // Since it is actually MY turn, if such a sequence exists, it means the board is in a state
    // where opponent *could* win if it was their turn.
    // This usually means there is an open 3 or 4.
    // If we find such a path, the first move is the critical spot.
    // We should play there.
    
    return BFS_FindWin(board, opponent, maxDepth);
}
