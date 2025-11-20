#include "bot.h"
#include "../search/ForcedBFS.h"
#include "../search/ForcedDFS.h"
#include "../search/PathReconstruction.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>
#include <iostream>
#include <limits>
#include <FL/Fl.H>
#include <thread>
#include <atomic>

// Thread-local random generator (independent per worker thread)
static thread_local std::mt19937 rng(std::random_device{}());

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

GomokuBot::~GomokuBot() {
    stopAnalysis();
}

void GomokuBot::setMode(BotMode mode) {
    currentMode = mode;
}

BotMode GomokuBot::getMode() const {
    return currentMode;
}

void GomokuBot::setSearchCallback(std::function<void(double, int, double, double)> cb) {
    statusCallback = cb;
}

std::optional<Move> GomokuBot::chooseMove(const Board& board, Player side) {
    if (board.isFull()) return std::nullopt;

    // 1. Check immediate threats (Win now or block loss)
    // This is critical for Gomoku to avoid silly mistakes
    if (auto threatMove = checkImmediateThreats(board, side)) {
        return threatMove;
    }

    // STEP 1: search forced wins (VCF)
    // Using depth 25 for VCF as it is narrower
    if (ForcedNode* forcedWin = BFS_FindWin(board, side, 25)) {
         auto path = reconstructPath(forcedWin);
         if (!path.empty()) return path.front();
    }

    // STEP 2: forced defense (VCF Block)
    // Checks if opponent has a VCF we need to block
    if (ForcedNode* forcedLose = BFS_FindLose(board, side, 17)) {
         auto path = reconstructPath(forcedLose);
         if (!path.empty()) {
             // path.front() is the opponent's winning move. We must block it.
             Move m = path.front();
             m.player = side; 
             return m;
         }
    }

    int durationMs = 2000; // Default Thinking

    switch (currentMode) {
        case BotMode::Instant:
            durationMs = 1000;
            break;
        case BotMode::Thinking:
            durationMs = 10000;
            break;
        case BotMode::ExtendedThinking:
            durationMs = 30000;
            break;
        case BotMode::Pro:
            durationMs = 60000;
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
            // Calculate progress based on durationMs
            double progress = 0.0;
            if (durationMs > 0) {
                progress = (double)elapsed / (double)durationMs;
                if (progress > 1.0) progress = 1.0;
            }
            
            statusCallback(winRate * 100.0, simulations, elapsedSec, progress);
            
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
        
        // Check pre-rollout state
        if (rolloutBoard.checkWinner() != Player::None) {
             winner = rolloutBoard.checkWinner();
        } else {
            while (movesMade < movesLimit) {
                if (rolloutBoard.isFull()) {
                    winner = Player::None;
                    break;
                }

                // Heavy rollout: pick moves near stones
                std::optional<Move> randomMove = getFastRandomMove(rolloutBoard, rolloutPlayer);
                if (!randomMove) break;
                
                rolloutBoard.placeStone(randomMove->row, randomMove->col, rolloutPlayer);
                
                Move lastMove = {randomMove->row, randomMove->col, rolloutPlayer};
                if (rolloutBoard.checkWinner(lastMove) == rolloutPlayer) {
                    winner = rolloutPlayer;
                    break;
                }
                
                rolloutPlayer = (rolloutPlayer == Player::Black) ? Player::White : Player::Black;
                movesMade++;
            }
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

void GomokuBot::stopAnalysis() {
    stopAnalysisFlag = true;
    if (analysisThread.joinable()) {
        analysisThread.join();
    }
    analysisRunning = false;
}

void GomokuBot::startAnalysis(const Board& board, Player side, std::function<void(double, int, double)> cb) {
    stopAnalysis(); // Stop any existing analysis
    stopAnalysisFlag = false;
    analysisRunning = true;

    analysisThread = std::thread([this, board, side, cb]() {
        auto startTime = std::chrono::steady_clock::now();
        
        // Root Setup
        Player opponent = (side == Player::Black) ? Player::White : Player::Black;
        auto root = std::make_unique<MCTSNode>(Move{-1, -1, Player::None}, nullptr, opponent);
        root->untriedMoves = getCandidateMoves(board, side);

        int simulations = 0;
        auto lastUpdate = std::chrono::steady_clock::now();
        double lastWinRatePercent = -1.0;

        while (!stopAnalysisFlag) {
            auto now = std::chrono::steady_clock::now();
            
            // MCTS Step (Selection, Expansion, Simulation, Backprop)
            // Duplicate logic from runMCTS but with interruption check
            
            MCTSNode* node = root.get();
            Board tempBoard = board;
            
            // Selection
            while (node->isFullyExpanded() && !node->children.empty()) {
                node = node->bestChild();
                tempBoard.placeStone(node->move.row, node->move.col, node->move.player);
            }

            // Expansion
            if (!node->isFullyExpanded() && !tempBoard.isFull()) {
                std::uniform_int_distribution<> dis(0, node->untriedMoves.size() - 1);
                int idx = dis(rng);
                Move move = node->untriedMoves[idx];
                node->untriedMoves.erase(node->untriedMoves.begin() + idx);

                Player currentPlayer = (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
                move.player = currentPlayer;
                
                auto child = std::make_unique<MCTSNode>(move, node, currentPlayer);
                tempBoard.placeStone(move.row, move.col, currentPlayer);
                
                Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
                child->untriedMoves = getCandidateMoves(tempBoard, nextPlayer);
                
                node->children.push_back(std::move(child));
                node = node->children.back().get();
            }

            // Rollout
            Player rolloutPlayer = (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
            Board rolloutBoard = tempBoard;
            Player winner = Player::None;
            int movesLimit = 225;
            int movesMade = 0;
            
            if (rolloutBoard.checkWinner() != Player::None) {
                winner = rolloutBoard.checkWinner();
            } else {
                while (movesMade < movesLimit) {
                    if (rolloutBoard.isFull()) break;

                    std::optional<Move> randomMove = getFastRandomMove(rolloutBoard, rolloutPlayer);
                    if (!randomMove) break;

                    rolloutBoard.placeStone(randomMove->row, randomMove->col, rolloutPlayer);
                    
                    Move lastMove = {randomMove->row, randomMove->col, rolloutPlayer};
                    if (rolloutBoard.checkWinner(lastMove) == rolloutPlayer) {
                        winner = rolloutPlayer;
                        break;
                    }

                    rolloutPlayer = (rolloutPlayer == Player::Black) ? Player::White : Player::Black;
                    movesMade++;
                }
            }

            // Backprop
            while (node != nullptr) {
                node->visits++;
                if (winner != Player::None) {
                    if (node->playerJustMoved == winner) node->wins += 1.0;
                    else if (node->playerJustMoved != Player::None) node->wins += 0.0;
                } else {
                    node->wins += 0.5;
                }
                node = node->parent;
            }
            simulations++;

            // Update Check
            auto updateDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
            if (updateDiff >= 100) { // 10 Hz update
                MCTSNode* best = root->bestChild(0);
                double winRate = 0.0;
                if (best && best->visits > 0) {
                    winRate = best->wins / best->visits;
                }
                
                double currentWinRatePercent = winRate * 100.0;
                
                // Stop if win rate stabilizes (change < 0.2% between updates)
                // We need a reasonable number of visits to ensure stability isn't just lack of samples
                if (best && best->visits > 200) { 
                     if (lastWinRatePercent >= 0.0) {
                         double diff = std::abs(currentWinRatePercent - lastWinRatePercent);
                         if (diff < 0.2) {
                             stopAnalysisFlag = true;
                         }
                     }
                     lastWinRatePercent = currentWinRatePercent;
                }

                double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;
                
                // Use Fl::awake to update UI safely from this thread
                // Copy values to capture by value
                struct UpdateData {
                    std::function<void(double, int, double)> cb;
                    double wr;
                    int sims;
                    double t;
                };
                auto* data = new UpdateData{cb, winRate * 100.0, simulations, elapsedSec};
                
                Fl::awake([](void* d) {
                    auto* data = (UpdateData*)d;
                    if (data->cb) data->cb(data->wr, data->sims, data->t);
                    delete data;
                }, data);

                lastUpdate = now;
            }
        }
    });
    
    // Do not detach. We want to join in stopAnalysis to ensure cleanup.
}

std::vector<Move> GomokuBot::getCandidateMoves(const Board& board, Player side) {
    std::vector<Move> moves;
    moves.reserve(50); 
    int size = board.size();
    
    // Optimization: Only scan relevant area
    int minR = size, maxR = -1, minC = size, maxC = -1;
    bool empty = true;
    for(int r=0; r<size; ++r) {
        for(int c=0; c<size; ++c) {
            if (!board.isEmpty(r,c)) {
                empty = false;
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
        }
    }
    
    if (empty) {
        moves.push_back({size/2, size/2, side});
        return moves;
    }
    
    // Expand bounds by 2
    minR = std::max(0, minR - 2);
    maxR = std::min(size-1, maxR + 2);
    minC = std::max(0, minC - 2);
    maxC = std::min(size-1, maxC + 2);

    for (int r = minR; r <= maxR; ++r) {
        for (int c = minC; c <= maxC; ++c) {
            if (board.isEmpty(r, c)) {
                // Check if it has neighbor within 2
                bool hasNeighbor = false;
                for (int dr = -2; dr <= 2 && !hasNeighbor; ++dr) {
                    for (int dc = -2; dc <= 2; ++dc) {
                         int nr = r + dr;
                         int nc = c + dc;
                         if (board.isInside(nr, nc) && !board.isEmpty(nr, nc)) {
                             hasNeighbor = true;
                             break;
                         }
                    }
                }
                if (hasNeighbor) {
                    moves.push_back({r, c, side});
                }
            }
        }
    }

    return moves;
}

std::optional<Move> GomokuBot::getFastRandomMove(const Board& board, Player side) {
    int size = board.size();
    // Try finding a random neighbor move by picking a random occupied cell and looking around it
    // If board is empty, center.
    
    // Optimization: Instead of scanning whole board for occupied cells, we just pick random coordinates
    // and see if they are valid.
    // BUT "valid" means "near other stones".
    
    // Better approach for speed:
    // Just collect occupied cells? No, too slow to do every time.
    
    // Hybrid approach:
    // Try 10 times to pick a random spot within the bounding box of current stones?
    // We don't track bounding box in Board.
    
    // Let's just do the Candidate Move logic but STOP after finding ONE random one?
    // No, that biases towards top-left.
    
    // We need a fast way to get a "neighboring empty cell".
    
    // Gather all occupied cells (fast enough for 15x15 usually < 100 stones)
    // If > 100 stones, board is half full, random picking works well.
    
    static thread_local std::vector<std::pair<int,int>> stones;
    stones.clear();
    stones.reserve(225);
    
    for(int r=0; r<size; ++r) {
        for(int c=0; c<size; ++c) {
            if(!board.isEmpty(r,c)) {
                stones.push_back({r,c});
            }
        }
    }
    
    if (stones.empty()) {
        return Move{size/2, size/2, side};
    }
    
    std::shuffle(stones.begin(), stones.end(), rng);
    
    // Try neighbors of random stones
    for(const auto& s : stones) {
        int r = s.first;
        int c = s.second;
        
        // Randomize neighbor order
        int drs[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        int dcs[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int indices[] = {0,1,2,3,4,5,6,7};
        
        // Partial shuffle of indices
        for (int i = 0; i < 8; ++i) {
             int j = std::uniform_int_distribution<>(i, 7)(rng);
             std::swap(indices[i], indices[j]);
        }
        
        for(int idx : indices) {
            int nr = r + drs[idx];
            int nc = c + dcs[idx];
            if(board.isInside(nr, nc) && board.isEmpty(nr, nc)) {
                return Move{nr, nc, side};
            }
        }
    }
    
    return std::nullopt;
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
    
    for (const auto& move : candidates) {
        Board temp = board;
        temp.placeStone(move.row, move.col, opp);
        
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
                return Move{move.row, move.col, side};
            }
             if (count == 4 && (openFront || openBack)) {
                return Move{move.row, move.col, side};
            }
        }
    }

    return std::nullopt;
}
