#include "ForcedBFS.h"

#include <FL/Fl.H>

#include <algorithm>
#include <deque>
#include <memory>
#include <queue>

#include "../bot/heuristics.h"

// Thread-local storage
static thread_local std::deque<std::unique_ptr<ForcedNode>> vcfNodePool;
static thread_local int nodesVisited = 0;

// Helpers
bool isFour(const Board& board, int r, int c, Player p) {
    // Check 4 directions
    int dr[] = {0, 1, 1, 1};
    int dc[] = {1, 0, 1, -1};

    for (int i = 0; i < 4; ++i) {
        int count = 1;
        // Forward
        int k = 1;
        while (board.isInside(r + k * dr[i], c + k * dc[i]) &&
               board.at(r + k * dr[i], c + k * dc[i]) == p) {
            count++;
            k++;
        }
        bool open1 = board.isInside(r + k * dr[i], c + k * dc[i]) &&
                     board.isEmpty(r + k * dr[i], c + k * dc[i]);

        // Backward
        int m = 1;
        while (board.isInside(r - m * dr[i], c - m * dc[i]) &&
               board.at(r - m * dr[i], c - m * dc[i]) == p) {
            count++;
            m++;
        }
        bool open2 = board.isInside(r - m * dr[i], c - m * dc[i]) &&
                     board.isEmpty(r - m * dr[i], c - m * dc[i]);

        // Live 4 (open both ends) or Broken 4 (open one end)
        // "Four" in VCF usually means a threat that wins next turn.
        // 4 stones in a row.
        if (count == 4) {
            if (open1 || open2) return true;
        }
        if (count == 5) return true;  // 5 is also good

        // Also check for "Gap 4"? X_XXX. Harder to detect with simple count.
        // We will trust simple Connect-4 logic for now.
    }
    return false;
}

// Returns moves that create a 4 or 5.
std::vector<Move> getVCFMoves(const Board& board, Player p) {
    std::vector<Move> candidates = Heuristics::getCandidateMoves(board, p);
    std::vector<Move> vcfMoves;
    vcfMoves.reserve(candidates.size());

    for (const auto& m : candidates) {
        Board temp = board;
        temp.placeStone(m.row, m.col, p);
        if (temp.checkWinner() == p) {
            vcfMoves.push_back(m);  // Winning move
        } else if (isFour(temp, m.row, m.col, p)) {
            vcfMoves.push_back(m);
        }
    }
    return vcfMoves;
}

// Get moves to block an immediate threat (4 or 5)
std::vector<Move> getBlockMoves(const Board& board, Player defender) {
    Player attacker = (defender == Player::Black) ? Player::White : Player::Black;
    std::vector<Move> blocks;

    // 1. Check if Attacker wins immediately (5)
    // If Attacker has 5, it's too late.
    // But we look for "Attacker HAS 3/4, and will win".
    // We need to find the squares that complete the 5.

    // Use checkImmediateThreats logic from Bot
    // But we need ALL blocking moves.

    std::vector<Move> candidates = Heuristics::getCandidateMoves(board, defender);

    // Identify threats by seeing where Attacker would play to win
    for (const auto& m : candidates) {
        Board temp = board;
        temp.placeStone(m.row, m.col, attacker);
        if (temp.checkWinner() == attacker) {
            // This spot is critical
            blocks.push_back({m.row, m.col, defender});
        }
    }

    return blocks;
}

ForcedNode* dfsVCF(ForcedNode* node, Player targetSide, int depth, int maxDepth) {
    if ((++nodesVisited & 0xFF) == 0) Fl::check();
    if (nodesVisited > 2000000) return nullptr;
    if (depth > maxDepth) return nullptr;

    Player current = node->toMove;

    if (current == targetSide) {
        // Attacker Turn: Try to find a move that creates 4 or 5
        std::vector<Move> moves = getVCFMoves(node->board, current);

        // Heuristic sorting: Win first
        // (Actually checkWinner in getVCFMoves handles this implicitly by order? No)
        // Let's sort winning moves to front?
        // For simplicity, just iterate.

        for (const auto& m : moves) {
            Board nextBoard = node->board;
            nextBoard.placeStone(m.row, m.col, m.player);

            if (nextBoard.checkWinner() == targetSide) {
                vcfNodePool.push_back(std::make_unique<ForcedNode>(
                    nextBoard, (current == Player::Black) ? Player::White : Player::Black, m, node,
                    depth + 1));
                return vcfNodePool.back().get();
            }

            // Create Child
            Player nextP = (current == Player::Black) ? Player::White : Player::Black;
            vcfNodePool.push_back(
                std::make_unique<ForcedNode>(nextBoard, nextP, m, node, depth + 1));
            ForcedNode* child = vcfNodePool.back().get();

            // Recurse
            ForcedNode* res = dfsVCF(child, targetSide, depth + 1, maxDepth);
            if (res) return res;  // OR node: one success is enough
        }
        return nullptr;

    } else {
        // Defender Turn: Must block immediate threats
        // Does Attacker have a threat?
        // In VCF, Attacker just played (in parent). So there MUST be a threat (Open 4 or 4).
        // We need to block it.

        std::vector<Move> blocks = getBlockMoves(node->board, current);

        if (blocks.empty()) {
            // No threats to block? Then Attacker failed to maintain VCF.
            // UNLESS Attacker has NO threats, which means Defender is safe.
            // So return nullptr (Attacker fails).
            return nullptr;
        }

        // AND node: All blocks must result in Attacker winning
        ForcedNode* lastRes = nullptr;
        for (const auto& m : blocks) {
            Board nextBoard = node->board;
            nextBoard.placeStone(m.row, m.col, m.player);

            if (nextBoard.checkWinner() == current) {
                // Defender counter-win?
                return nullptr;  // Attacker fails
            }

            Player nextP = (current == Player::Black) ? Player::White : Player::Black;
            vcfNodePool.push_back(
                std::make_unique<ForcedNode>(nextBoard, nextP, m, node, depth + 1));
            ForcedNode* child = vcfNodePool.back().get();

            ForcedNode* res = dfsVCF(child, targetSide, depth + 1, maxDepth);
            if (!res) return nullptr;  // One defense worked, so Attacker fails
            lastRes = res;
        }
        return lastRes;  // All defenses failed, return one leaf
    }
}

ForcedNode* BFS_FindWin(const Board& board, Player side, int maxDepth) {
    vcfNodePool.clear();
    nodesVisited = 0;

    // Iterative Deepening for VCF
    // VCF depth can be deep (15+), but we start small
    for (int d = 1; d <= maxDepth; d += 2) {
        vcfNodePool.clear();  // Reset pool for new depth (optimization)
        nodesVisited = 0;

        vcfNodePool.push_back(
            std::make_unique<ForcedNode>(board, side, Move{-1, -1, Player::NoPlayer}, nullptr, 0));
        ForcedNode* root = vcfNodePool.back().get();

        ForcedNode* res = dfsVCF(root, side, 0, d);
        if (res) return res;
    }

    return nullptr;
}

ForcedNode* BFS_FindLose(const Board& board, Player side, int maxDepth) {
    Player opponent = (side == Player::Black) ? Player::White : Player::Black;
    return BFS_FindWin(board, opponent, maxDepth);
}
