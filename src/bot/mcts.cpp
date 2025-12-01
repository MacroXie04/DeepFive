#include "mcts.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>

// Thread-local random generator
static thread_local std::mt19937 rng(std::random_device{}());

MCTSNode::MCTSNode(Move m, MCTSNode* p, Player justMoved)
    : move(m), parent(p), visits(0), wins(0.0), playerJustMoved(justMoved) {}

bool MCTSNode::isFullyExpanded() const {
    return untriedMoves.empty();
}

MCTSNode* MCTSNode::bestChild(double explorationValue) {
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

MCTSSolver::MCTSSolver(const Board& board, Player side) : rootBoard(board) {
    Player opponent = (side == Player::Black) ? Player::White : Player::Black;
    root = std::make_unique<MCTSNode>(Move{-1, -1, Player::NoPlayer}, nullptr, opponent);
    root->untriedMoves = Heuristics::getCandidateMoves(board, side);
}

void MCTSSolver::step() {
    MCTSNode* node = root.get();
    Board tempBoard = rootBoard;

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
        
        // O(1) removal: swap with last element and pop
        std::swap(node->untriedMoves[idx], node->untriedMoves.back());
        node->untriedMoves.pop_back();

        Player currentPlayer =
            (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
        move.player = currentPlayer;

        auto child = std::make_unique<MCTSNode>(move, node, currentPlayer);
        tempBoard.placeStone(move.row, move.col, currentPlayer);

        Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
        child->untriedMoves = Heuristics::getCandidateMoves(tempBoard, nextPlayer);

        node->children.push_back(std::move(child));
        node = node->children.back().get();
    }

    // Simulation (Rollout) with fast heuristic guidance
    Player rolloutPlayer = (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
    Board rolloutBoard = tempBoard;
    Player winner = Player::NoPlayer;

    int movesLimit = 225;
    int movesMade = 0;

    if (rolloutBoard.checkWinner() != Player::NoPlayer) {
        winner = rolloutBoard.checkWinner();
    } else {
        while (movesMade < movesLimit) {
            if (rolloutBoard.isFull()) {
                winner = Player::NoPlayer;
                break;
            }

            // Try fast threat move first (win or block) - O(n) not O(n²)
            std::optional<Move> selectedMove = 
                Heuristics::getFastThreatMove(rolloutBoard, rolloutPlayer);
            
            // Fall back to random move if no immediate threat
            if (!selectedMove) {
                selectedMove = Heuristics::getFastRandomMove(rolloutBoard, rolloutPlayer);
            }
            
            if (!selectedMove) break;

            rolloutBoard.placeStone(selectedMove->row, selectedMove->col, rolloutPlayer);

            Move lastMove = {selectedMove->row, selectedMove->col, rolloutPlayer};
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
        if (winner != Player::NoPlayer) {
            if (node->playerJustMoved == winner) {
                node->wins += 1.0;
            } else if (node->playerJustMoved != Player::NoPlayer) {
                node->wins += 0.0;
            }
        } else {
            node->wins += 0.5;
        }
        node = node->parent;
    }

    totalSimulations++;
}

void MCTSSolver::run(int durationMs, std::function<void(double, int, double, double)> statusCb) {
    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = startTime;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= durationMs) break;

        step();

        auto updateDiff =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        if (updateDiff >= 100 && statusCb) {
            double winRate = 0.0;
            MCTSNode* best = root->bestChild(0);
            if (best && best->visits > 0) {
                winRate = best->wins / best->visits;
            }

            double elapsedSec = elapsed / 1000.0;
            double progress = 0.0;
            if (durationMs > 0) {
                progress = (double)elapsed / (double)durationMs;
                if (progress > 1.0) progress = 1.0;
            }

            statusCb(winRate * 100.0, totalSimulations, elapsedSec, progress);
            lastUpdate = now;
        }
    }
}

void MCTSSolver::runContinuous(std::atomic<bool>& stopFlag,
                               std::function<void(double, int, double)> statusCb) {
    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = startTime;
    double lastWinRatePercent = -1.0;
    int stableCount = 0;

    while (!stopFlag) {
        step();

        auto now = std::chrono::steady_clock::now();
        auto updateDiff =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();

        if (updateDiff >= 100) {
            MCTSNode* best = root->bestChild(0);
            double winRate = 0.0;
            if (best && best->visits > 0) {
                winRate = best->wins / best->visits;
            }

            double currentWinRatePercent = winRate * 100.0;

            // Early termination: position is clearly decisive
            if (best && best->visits > 100) {
                if (currentWinRatePercent > 95.0 || currentWinRatePercent < 5.0) {
                    // Report final status and stop
                    double elapsedSec =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() /
                        1000.0;
                    if (statusCb) {
                        statusCb(currentWinRatePercent, totalSimulations, elapsedSec);
                    }
                    stopFlag = true;
                    return;
                }
            }

            // Stop if win rate stabilizes (require 3 consecutive stable readings)
            if (best && best->visits > 200) {
                if (lastWinRatePercent >= 0.0) {
                    double diff = std::abs(currentWinRatePercent - lastWinRatePercent);
                    if (diff < 0.3) {
                        stableCount++;
                        if (stableCount >= 3) {
                            stopFlag = true;
                        }
                    } else {
                        stableCount = 0;
                    }
                }
                lastWinRatePercent = currentWinRatePercent;
            }

            double elapsedSec =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() /
                1000.0;

            if (statusCb) {
                statusCb(currentWinRatePercent, totalSimulations, elapsedSec);
            }

            lastUpdate = now;
        }
    }
}

std::pair<std::optional<Move>, int> MCTSSolver::getBestMove() {
    MCTSNode* bestNode = nullptr;
    int maxVisits = -1;
    for (const auto& child : root->children) {
        if (child->visits > maxVisits) {
            maxVisits = child->visits;
            bestNode = child.get();
        }
    }

    if (bestNode) {
        return {bestNode->move, totalSimulations};
    }
    return {std::nullopt, totalSimulations};
}

std::vector<MCTSSolver::CandidateEval> MCTSSolver::getCandidateEvaluations() const {
    std::vector<CandidateEval> evals;
    if (!root || root->children.empty()) return evals;

    // Find max visits for normalization
    int maxVisits = 0;
    for (const auto& child : root->children) {
        if (child->visits > maxVisits) {
            maxVisits = child->visits;
        }
    }

    if (maxVisits == 0) return evals;

    for (const auto& child : root->children) {
        if (child->visits > 0) {
            CandidateEval eval;
            eval.move = child->move;
            eval.visits = child->visits;
            // Score based on win rate and visit count
            double winRate = child->wins / child->visits;
            double visitRatio = (double)child->visits / maxVisits;
            // Combine win rate (70%) and visit ratio (30%) for visual prominence
            eval.score = (float)(winRate * 0.7 + visitRatio * 0.3);
            evals.push_back(eval);
        }
    }

    return evals;
}
