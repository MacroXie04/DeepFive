#include "bot.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>
#include <iostream>
#include <limits>
#include <FL/Fl.H>

// Random generator
static std::mt19937 rng(std::random_device{}());

struct MCTSNode {
    Move move;
    MCTSNode* parent;
    std::vector<std::unique_ptr<MCTSNode>> children;
    int visits;
    double wins;
    std::vector<Move> untriedMoves;
    Player playerJustMoved;

    MCTSNode(Move m, MCTSNode* p, Player justMoved) 
        : move(m), parent(p), visits(0), wins(0.0), playerJustMoved(justMoved) {}

    bool isFullyExpanded() const {
        return untriedMoves.empty();
    }

    MCTSNode* bestChild(double explorationValue = 1.41) {
        MCTSNode* best = nullptr;
        double bestValue = -std::numeric_limits<double>::max();

        for (const auto& child : children) {
            double uctValue = (child->wins / (double)child->visits) +
                              explorationValue * std::sqrt(std::log(visits) / (double)child->visits);
            if (uctValue > bestValue) {
                bestValue = uctValue;
                best = child.get();
            }
        }
        return best;
    }
};

GomokuBot::GomokuBot() : currentMode(BotMode::Thinking) {}

void GomokuBot::setMode(BotMode mode) {
    currentMode = mode;
}

BotMode GomokuBot::getMode() const {
    return currentMode;
}

void GomokuBot::setSearchCallback(std::function<void(double, int, double)> cb) {
    statusCallback = cb;
}

std::optional<Move> GomokuBot::chooseMove(const Board& board, Player side) {
    if (board.isFull()) return std::nullopt;

    // 1. Check immediate threats (Win now or block loss)
    // This is critical for Gomoku to avoid silly mistakes
    if (auto threatMove = checkImmediateThreats(board, side)) {
        return threatMove;
    }

    int durationMs = 2000; // Default Thinking

    switch (currentMode) {
        case BotMode::Instant:
            durationMs = 200;
            break;
        case BotMode::Thinking:
            durationMs = 2000;
            break;
        case BotMode::Extended:
            durationMs = 10000;
            break;
        case BotMode::Auto: {
            // Dynamic logic
            int stoneCount = 0;
            int size = board.size();
            for (int r=0; r<size; ++r)
                for (int c=0; c<size; ++c)
                    if (!board.isEmpty(r,c)) stoneCount++;
            
            if (stoneCount < 10) {
                durationMs = 500; // Early game fast
            } else if (stoneCount < 30) {
                durationMs = 2000; // Mid game normal
            } else {
                durationMs = 5000; // Late game think harder
            }
            break;
        }
    }

    auto result = runMCTS(board, side, durationMs);
    return result.first;
}

int GomokuBot::runBenchmark(int durationMs) {
    // Empty-ish board for consistent benchmark
    Board board(15);
    board.placeStone(7, 7, Player::Black);
    
    // Run MCTS silently
    auto result = runMCTS(board, Player::White, durationMs);
    
    int sims = result.second;
    // SPS = sims / (duration / 1000)
    return (int)((double)sims / ((double)durationMs / 1000.0));
}

std::pair<std::optional<Move>, int> GomokuBot::runMCTS(const Board& board, Player side, int durationMs) {
    auto startTime = std::chrono::steady_clock::now();

    // Root node - "move" is irrelevant for root, but playerJustMoved should be opponent
    Player opponent = (side == Player::Black) ? Player::White : Player::Black;
    auto root = std::make_unique<MCTSNode>(Move{-1, -1, Player::None}, nullptr, opponent);
    root->untriedMoves = getCandidateMoves(board, side);

    int simulations = 0;
    auto lastUpdate = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= durationMs) break;

        // UI Update & System Stats every 100ms
        // Only call callback if set
        auto updateDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        if (updateDiff >= 100 && statusCallback) {
            // Calculate win rate of best child
            double winRate = 0.0;
            MCTSNode* best = root->bestChild(0); // pure exploitation for stats
            if (best && best->visits > 0) {
                winRate = best->wins / best->visits;
            }

            double elapsedSec = elapsed / 1000.0;
            statusCallback(winRate * 100.0, simulations, elapsedSec);
            
            Fl::check(); // Keep UI responsive
            lastUpdate = now;
        }

        // Selection
        MCTSNode* node = root.get();
        Board tempBoard = board;
        
        // Descend tree
        while (node->isFullyExpanded() && !node->children.empty()) {
            node = node->bestChild();
            tempBoard.placeStone(node->move.row, node->move.col, node->move.player);
        }

        // Expansion
        if (!node->isFullyExpanded() && !tempBoard.isFull()) {
            // Pick random untried move
            std::uniform_int_distribution<> dis(0, node->untriedMoves.size() - 1);
            int idx = dis(rng);
            Move move = node->untriedMoves[idx];
            node->untriedMoves.erase(node->untriedMoves.begin() + idx);

            Player currentPlayer = (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
            move.player = currentPlayer;
            
            auto child = std::make_unique<MCTSNode>(move, node, currentPlayer);
            tempBoard.placeStone(move.row, move.col, currentPlayer);
            
            // Generate candidates for the new child
            Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
            child->untriedMoves = getCandidateMoves(tempBoard, nextPlayer);
            
            node->children.push_back(std::move(child));
            node = node->children.back().get();
        }

        // Simulation (Rollout)
        Player rolloutPlayer = (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
        Board rolloutBoard = tempBoard;
        Player winner = Player::None;
        
        int movesLimit = 225; // Max board size
        int movesMade = 0;
        
        while (movesMade < movesLimit) {
            if (rolloutBoard.checkWinner() != Player::None) {
                winner = rolloutBoard.checkWinner();
                break;
            }
            if (rolloutBoard.isFull()) {
                winner = Player::None;
                break;
            }

            // Heavy rollout: pick moves near stones
            auto candidates = getCandidateMoves(rolloutBoard, rolloutPlayer);
            if (candidates.empty()) break; // Should not happen if not full
            
            std::uniform_int_distribution<> dis(0, candidates.size() - 1);
            Move randomMove = candidates[dis(rng)];
            
            rolloutBoard.placeStone(randomMove.row, randomMove.col, rolloutPlayer);
            rolloutPlayer = (rolloutPlayer == Player::Black) ? Player::White : Player::Black;
            movesMade++;
        }

        // Backpropagation
        while (node != nullptr) {
            node->visits++;
            if (winner != Player::None) {
                // If the winner is the player who just moved at this node, it's a win for this node
                if (node->playerJustMoved == winner) {
                    node->wins += 1.0;
                } else if (node->playerJustMoved != Player::None) {
                    // Loss for this node
                    node->wins += 0.0; 
                }
            } else {
                // Draw
                node->wins += 0.5;
            }
            node = node->parent;
        }
        
        simulations++;
    }

    // Select best move (most visited)
    MCTSNode* bestNode = nullptr;
    int maxVisits = -1;
    for (const auto& child : root->children) {
        if (child->visits > maxVisits) {
            maxVisits = child->visits;
            bestNode = child.get();
        }
    }

    if (bestNode) {
        return {bestNode->move, simulations};
    }
    return {std::nullopt, simulations};
}

std::vector<Move> GomokuBot::getCandidateMoves(const Board& board, Player side) {
    std::vector<Move> moves;
    moves.reserve(50); // Optimization
    int size = board.size();
    std::vector<bool> visited(size * size, false);
    bool emptyBoard = true;

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (!board.isEmpty(r, c)) {
                emptyBoard = false;
                // Check neighbors within radius 2
                for (int dr = -2; dr <= 2; ++dr) {
                    for (int dc = -2; dc <= 2; ++dc) {
                        int nr = r + dr;
                        int nc = c + dc;
                        if (board.isInside(nr, nc) && board.isEmpty(nr, nc)) {
                            if (!visited[nr * size + nc]) {
                                visited[nr * size + nc] = true;
                                moves.push_back({nr, nc, side});
                            }
                        }
                    }
                }
            }
        }
    }

    if (emptyBoard) {
        moves.push_back({size / 2, size / 2, side});
    }
    
    return moves;
}

// Simplified check for 5-in-row or blocking 4-in-row
std::optional<Move> GomokuBot::checkImmediateThreats(const Board& board, Player side) {
    Player opp = (side == Player::Black) ? Player::White : Player::Black;
    auto candidates = getCandidateMoves(board, side);
    
    // 1. Can we win directly?
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, side);
        if (temp.checkWinner() == side) {
            return move;
        }
    }

    // 2. Must we block opponent win (Connect 4)?
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, opp);
        if (temp.checkWinner() == opp) {
            return Move{move.row, move.col, side}; // Block it!
        }
    }
    
    // 3. Must we block opponent Open 3? (To prevent Open 4)
    // An "Open 3" is a 3-in-row with two open ends. 
    // If we don't block it, opponent plays one end to make Open 4, which is unstoppable.
    // We can check this by seeing if opponent can win in 2 moves (Move 1: make Open 4, Move 2: Win)
    // A simplified check: If placing a stone here creates a win for opponent on the NEXT turn regardless of my move? No that's too complex.
    // Simpler: Check patterns.
    // Or: If opponent places here, do they have an Open 4?
    // Open 4 means: 4 stones, and both ends are empty.
    // Wait, checkWinner only checks 5.
    
    // Let's iterate moves. If opponent places stone, and it results in an "Open 4", we MUST block this spot (or the other end).
    // Actually, if opponent has an Open 3, there are TWO spots they can play to make an Open 4.
    // If we block one, they might play the other.
    // But usually blocking one end of an Open 3 forces them to respond or converts it to a Closed 4 which is less dangerous.
    
    // Let's try: If opponent places at 'move', does it create an Open 4?
    // How to detect Open 4? 
    // It's 4 stones in a row, and the ends (length+1) are empty.
    // We can add a helper "isOpen4" but we don't have easy access to board internals here.
    // Let's stick to the plan: Detect if opponent can create a "Unstoppable Threat".
    // For now, let's just extend the block check.
    
    // Improved Heuristic:
    // If opponent playing at 'move' creates TWO distinct winning lines (3-3 or 4-3), we must block.
    // But simpler for now:
    // Let's just look for "Live 3" blocking. 
    
    // Implementation:
    // We simulate opponent move. Then we check if that move created a state where opponent has a winning threat that requires TWO blocks (which is impossible).
    // Actually, let's just increase the depth of "Immediate Threat".
    
    // If opponent moves here, do they win on the NEXT turn? (We already checked this with Block Win 4).
    // What we want is: If opponent moves here, do they have a threat that guarantees a win in 2 turns?
    
    // Let's keep it efficient. We will trust MCTS for complex traps, but Open 3 is a standard shape.
    // Let's hardcode Open 3 detection?
    // 01110 -> 011110 (Open 4).
    
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, opp);
        
        // Check if this move created an Open 4 for the opponent.
        // An Open 4 is a winning state because the next move wins.
        // Actually, if they have Open 4, `checkWinner` won't trigger yet.
        // But if they have Open 4, we have lost effectively.
        // So we must block the move that Creates the Open 4.
        
        // How to check for Open 4 efficiently using Board?
        // We can scan the lines around the placed stone.
        int r = move.row;
        int c = move.col;
        int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
        
        for (auto& dir : directions) {
            int count = 1;
            // Check forward
            int i = 1;
            while (temp.isInside(r + i*dir[0], c + i*dir[1]) && temp.at(r + i*dir[0], c + i*dir[1]) == opp) {
                count++;
                i++;
            }
            bool openFront = temp.isInside(r + i*dir[0], c + i*dir[1]) && temp.isEmpty(r + i*dir[0], c + i*dir[1]);
            
            // Check backward
            int j = 1;
            while (temp.isInside(r - j*dir[0], c - j*dir[1]) && temp.at(r - j*dir[0], c - j*dir[1]) == opp) {
                count++;
                j++;
            }
            bool openBack = temp.isInside(r - j*dir[0], c - j*dir[1]) && temp.isEmpty(r - j*dir[0], c - j*dir[1]);
            
            if (count == 4 && openFront && openBack) {
                // Opponent made an Open 4! We must block this `move` preventatively!
                return Move{move.row, move.col, side};
            }
             if (count == 4 && (openFront || openBack)) {
                // Opponent made a Closed 4 (4 in a row with one open end).
                // This is also a "must block" usually, but covered by "Block Win" check above?
                // No, "Block Win" checks if they *already* have 4 and are playing the 5th.
                // This check is: They are playing the 4th stone now.
                // If they play the 4th stone, and it's open, they win next turn.
                // So YES, we must block any move that creates a 4-in-row.
                return Move{move.row, move.col, side};
            }
            
            // Also Block Open 3? (which becomes Open 4)
            // If count == 3 and OpenFront and OpenBack -> Open 3.
            // If opponent plays to make an Open 3, it's dangerous.
            // But usually MCTS can handle 3s. 4s are critical.
            // Let's stick to blocking 4s (Live 4 or Dead 4) created by this move.
            
            // Actually, the previous block (Win 4) handles the case where opponent HAS 4 and plays 5.
            // This block handles the case where opponent HAS 3 and plays 4.
        }
    }

    return std::nullopt;
}
