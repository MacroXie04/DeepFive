#include "ForcedDFS.h"

#include <deque>
#include <memory>

#include "../bot/heuristics.h"

// Thread-local storage for DFS nodes
// Use std::deque to ensure pointers to elements remain valid upon insertion.
thread_local std::deque<std::unique_ptr<ForcedNode>> dfsNodePool;
thread_local int dfsNodesVisited = 0;

// Thread-local storage for VCF solver
thread_local std::deque<std::unique_ptr<ForcedNode>> vcfNodePool;
thread_local int vcfNodesVisited = 0;

// Check if placing a stone creates a "Four" (4 in a row with at least one open end)
static bool createsFour(const Board& board, int row, int col, Player player) {
    auto threat = Patterns::analyzeMoveThreat(board, row, col, player);
    return threat.createsFive || threat.counts.totalFours() > 0;
}

// Get VCF moves: for attacker, moves that create a Four; for defender, moves that block attacker's
// Four
static std::vector<Move> getVCFMoves(const Board& board, Player currentPlayer, Player attacker) {
    std::vector<Move> moves;
    auto candidates = Heuristics::getCandidateMoves(board, currentPlayer);

    if (currentPlayer == attacker) {
        // Attacker: find moves that create a Four
        for (const auto& mv : candidates) {
            Board temp = board;
            temp.placeStone(mv.row, mv.col, currentPlayer);

            // Check if this move wins immediately
            if (temp.checkWinner() == currentPlayer) {
                moves.clear();
                moves.push_back(mv);
                return moves;  // Winning move found, return immediately
            }

            // Check if this creates a Four
            if (createsFour(temp, mv.row, mv.col, currentPlayer)) {
                moves.push_back(mv);
            }
        }
    } else {
        // Defender: find moves that block attacker's Four
        for (const auto& mv : candidates) {
            // Check if attacker would win here
            Board temp = board;
            temp.placeStone(mv.row, mv.col, attacker);
            if (temp.checkWinner() == attacker) {
                // Must block this position
                moves.push_back(mv);
            }
        }
    }

    return moves;
}

ForcedNode* DFS_Recursive(ForcedNode* node, Player targetSide, int maxDepth,
                          const std::function<void()>& eventPump) {
    if ((++dfsNodesVisited & 0xFF) == 0) {
        if (eventPump) eventPump();
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
            dfsNodePool.push_back(
                std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
            return dfsNodePool.back().get();
        }

        Player winner = newBoard.checkWinner();
        if (winner != Player::NoPlayer && winner != targetSide) {
            continue;  // Loss branch
        }

        dfsNodePool.push_back(
            std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
        ForcedNode* child = dfsNodePool.back().get();

        ForcedNode* result = DFS_Recursive(child, targetSide, maxDepth, eventPump);
        if (result != nullptr) return result;
    }

    return nullptr;
}

ForcedNode* DFS_FindWin(const Board& board, Player side, int maxDepth,
                        const std::function<void()>& eventPump) {
    dfsNodePool.clear();
    dfsNodesVisited = 0;

    // Root
    dfsNodePool.push_back(
        std::make_unique<ForcedNode>(board, side, Move{-1, -1, Player::NoPlayer}, nullptr, 0));
    ForcedNode* root = dfsNodePool.back().get();

    return DFS_Recursive(root, side, maxDepth, eventPump);
}

// VCF recursive search
static ForcedNode* VCF_Recursive(ForcedNode* node, Player attacker, int maxDepth,
                                 const std::function<void()>& eventPump) {
    if ((++vcfNodesVisited & 0xFF) == 0) {
        if (eventPump) eventPump();
    }
    if (vcfNodesVisited > 100000) return nullptr;  // Node limit
    if (node->depth >= maxDepth) return nullptr;

    Player currentPlayer = node->toMove;
    Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;

    std::vector<Move> moves = getVCFMoves(node->board, currentPlayer, attacker);

    // If no forcing moves available, VCF fails at this branch
    if (moves.empty()) return nullptr;

    for (const auto& mv : moves) {
        Board newBoard = node->board;
        newBoard.placeStone(mv.row, mv.col, mv.player);

        // Check for immediate win
        if (newBoard.checkWinner() == attacker) {
            vcfNodePool.push_back(
                std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
            return vcfNodePool.back().get();
        }

        // Check for loss
        Player winner = newBoard.checkWinner();
        if (winner != Player::NoPlayer && winner != attacker) {
            continue;  // Loss branch, skip
        }

        vcfNodePool.push_back(
            std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
        ForcedNode* child = vcfNodePool.back().get();

        ForcedNode* result = VCF_Recursive(child, attacker, maxDepth, eventPump);
        if (result != nullptr) return result;
    }

    return nullptr;
}

ForcedNode* VCF_Solve(const Board& board, Player side, int maxDepth,
                      const std::function<void()>& eventPump) {
    vcfNodePool.clear();
    vcfNodesVisited = 0;

    // Root node
    vcfNodePool.push_back(
        std::make_unique<ForcedNode>(board, side, Move{-1, -1, Player::NoPlayer}, nullptr, 0));
    ForcedNode* root = vcfNodePool.back().get();

    return VCF_Recursive(root, side, maxDepth, eventPump);
}

// ============== VCT (Victory by Continuous Threes) ==============

thread_local std::deque<std::unique_ptr<ForcedNode>> vctNodePool;
thread_local int vctNodesVisited = 0;

// Check if placing creates a live three (活三: 3 stones with 2 open ends)
static bool createsLiveThree(const Board& board, int row, int col, Player player) {
    auto threat = Patterns::analyzeMoveThreat(board, row, col, player);
    return threat.counts.liveThrees > 0 || threat.counts.jumpLiveThrees > 0 ||
           threat.createsDoubleThree;
}

// Get VCT moves: attacks create live three or rush four; defense blocks these
static std::vector<Move> getVCTMoves(const Board& board, Player currentPlayer, Player attacker) {
    std::vector<Move> moves;
    auto candidates = Heuristics::getCandidateMoves(board, currentPlayer);

    if (currentPlayer == attacker) {
        // Attacker: find moves that create live three or rush four
        for (const auto& mv : candidates) {
            Board temp = board;
            temp.placeStone(mv.row, mv.col, currentPlayer);

            // Check if wins
            if (temp.checkWinner() == currentPlayer) {
                moves.clear();
                moves.push_back(mv);
                return moves;
            }

            // Check if creates four (forcing)
            if (createsFour(temp, mv.row, mv.col, currentPlayer)) {
                moves.push_back(mv);
                continue;
            }

            // Check if creates live three (semi-forcing)
            if (createsLiveThree(temp, mv.row, mv.col, currentPlayer)) {
                moves.push_back(mv);
            }
        }
    } else {
        // Defender: block attacker's four or live three
        for (const auto& mv : candidates) {
            // Must block if attacker would win here
            Board temp = board;
            temp.placeStone(mv.row, mv.col, attacker);
            if (temp.checkWinner() == attacker) {
                moves.push_back(mv);
                continue;
            }

            // Block if creates four
            if (createsFour(temp, mv.row, mv.col, attacker)) {
                moves.push_back(mv);
            }
        }

        // If no forced blocks, defender can play anywhere reasonable
        if (moves.empty()) {
            return candidates;
        }
    }

    return moves;
}

static ForcedNode* VCT_Recursive(ForcedNode* node, Player attacker, int maxDepth,
                                 const std::function<void()>& eventPump) {
    if ((++vctNodesVisited & 0xFF) == 0) {
        if (eventPump) eventPump();
    }
    if (vctNodesVisited > 50000) return nullptr;  // Lower limit for VCT
    if (node->depth >= maxDepth) return nullptr;

    Player currentPlayer = node->toMove;
    Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;

    std::vector<Move> moves = getVCTMoves(node->board, currentPlayer, attacker);

    if (moves.empty()) return nullptr;

    for (const auto& mv : moves) {
        Board newBoard = node->board;
        newBoard.placeStone(mv.row, mv.col, mv.player);

        if (newBoard.checkWinner() == attacker) {
            vctNodePool.push_back(
                std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
            return vctNodePool.back().get();
        }

        Player winner = newBoard.checkWinner();
        if (winner != Player::NoPlayer && winner != attacker) {
            continue;
        }

        vctNodePool.push_back(
            std::make_unique<ForcedNode>(newBoard, nextPlayer, mv, node, node->depth + 1));
        ForcedNode* child = vctNodePool.back().get();

        ForcedNode* result = VCT_Recursive(child, attacker, maxDepth, eventPump);
        if (result != nullptr) return result;
    }

    return nullptr;
}

ForcedNode* VCT_Solve(const Board& board, Player side, int maxDepth,
                      const std::function<void()>& eventPump) {
    vctNodePool.clear();
    vctNodesVisited = 0;

    vctNodePool.push_back(
        std::make_unique<ForcedNode>(board, side, Move{-1, -1, Player::NoPlayer}, nullptr, 0));
    ForcedNode* root = vctNodePool.back().get();

    return VCT_Recursive(root, side, maxDepth, eventPump);
}
