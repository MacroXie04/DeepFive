#include "bot.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>
#include <iostream>
#include <limits>
#include <FL/Fl.H>
#include <thread>
#include <atomic>

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
            while (movesMade < movesLimit) {
                if (rolloutBoard.checkWinner() != Player::None) {
                    winner = rolloutBoard.checkWinner();
                    break;
                }
                if (rolloutBoard.isFull()) break;

                auto candidates = getCandidateMoves(rolloutBoard, rolloutPlayer);
                if (candidates.empty()) break;
                std::uniform_int_distribution<> dis(0, candidates.size() - 1);
                Move randomMove = candidates[dis(rng)];
                rolloutBoard.placeStone(randomMove.row, randomMove.col, rolloutPlayer);
                rolloutPlayer = (rolloutPlayer == Player::Black) ? Player::White : Player::Black;
                movesMade++;
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
                
                // Calculate Error Margin (approximate standard error for proportion)
                // SE = sqrt(p(1-p)/n)
                // We stop when 1.96 * SE * 100 < 0.5
                // If visits is small, error is large.
                double p = winRate;
                if (best && best->visits > 50) { // Min visits to trust
                     double se = std::sqrt(p * (1.0 - p) / best->visits);
                     double marginError = 1.96 * se * 100.0;
                     
                     if (marginError < 0.5) {
                         // Convergence reached
                         stopAnalysisFlag = true;
                     }
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
