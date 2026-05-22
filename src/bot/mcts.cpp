#include "mcts.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

// Rollout configuration
constexpr double PROGRESSIVE_BIAS_SCALE = 0.5;  // Scale factor for progressive bias

MCTSNode::MCTSNode(Move m, MCTSNode* p, Player justMoved, double bias, int score, uint64_t hash)
    : move(m),
      parent(p),
      visits(0),
      wins(0.0),
      playerJustMoved(justMoved),
      heuristicBias(bias),
      heuristicScore(score),
      stateHash(hash) {}

bool MCTSNode::isFullyExpanded() const {
    return untriedMoves.empty();
}

MCTSNode* MCTSNode::bestChild(double explorationValue, bool useProgressiveBias) {
    MCTSNode* best = nullptr;
    double bestValue = -std::numeric_limits<double>::max();

    for (const auto& child : children) {
        // Standard UCT formula
        double exploitation = child->wins / (double)child->visits;
        double exploration = explorationValue * std::sqrt(std::log(visits) / (double)child->visits);

        // Progressive bias: decreases as visits increase
        // H / (visits + 1) where H is the heuristic bias
        double progressiveBias = 0.0;
        if (useProgressiveBias && child->heuristicBias > 0) {
            progressiveBias = PROGRESSIVE_BIAS_SCALE * child->heuristicBias / (child->visits + 1);
        }

        double uctValue = exploitation + exploration + progressiveBias;

        if (uctValue > bestValue) {
            bestValue = uctValue;
            best = child.get();
        }
    }
    return best;
}

MCTSSolver::MCTSSolver(const Board& board, Player side) : MCTSSolver(board, side, Options{}) {}

MCTSSolver::MCTSSolver(const Board& board, Player side, Options options)
    : rootBoard(board), options(options) {
    if (this->options.seed) {
        rng.seed(*this->options.seed);
        Heuristics::setRandomSeed(*this->options.seed);
    } else {
        std::random_device rd;
        rng.seed(rd());
    }

    Player opponent = (side == Player::Black) ? Player::White : Player::Black;
    root = std::make_unique<MCTSNode>(Move{-1, -1, Player::NoPlayer}, nullptr, opponent, 0.0, 0,
                                      board.getHash());
    root->untriedMoves = getOrderedMoves(board, side, this->options.rootBeamSize);
}

void MCTSSolver::setSeed(uint32_t seed) {
    options.seed = seed;
    rng.seed(seed);
    Heuristics::setRandomSeed(seed);
}

uint64_t MCTSSolver::getTranspositionKey(uint64_t boardHash, Player side) const {
    uint64_t sideValue = (side == Player::Black) ? 0x9E3779B97F4A7C15ULL : 0xBF58476D1CE4E5B9ULL;
    return boardHash ^ (sideValue + (boardHash << 6) + (boardHash >> 2));
}

std::vector<Move> MCTSSolver::getOrderedMoves(const Board& board, Player side, int maxMoves) {
    uint64_t key = getTranspositionKey(board.getHash(), side) ^ ((uint64_t)maxMoves << 48);
    if (options.enableTranspositionTable) {
        auto it = transpositionTable.find(key);
        if (it != transpositionTable.end() && it->second.hasCandidates) {
            return it->second.candidates;
        }
    }

    Heuristics::CandidateOptions candidateOptions;
    candidateOptions.maxMoves = maxMoves;
    candidateOptions.preserveForcingMoves = true;
    auto scoredMoves = Heuristics::getScoredCandidateMoves(board, side, candidateOptions);

    std::vector<Move> moves;
    moves.reserve(scoredMoves.size());
    for (const auto& scored : scoredMoves) {
        moves.push_back(scored.move);
    }

    if (options.enableTranspositionTable) {
        auto& entry = transpositionTable[key];
        entry.hasCandidates = true;
        entry.candidates = moves;
    }

    return moves;
}

double MCTSSolver::evaluateStaticBlackWinProbability(const Board& board) {
    uint64_t key = getTranspositionKey(board.getHash(), Player::Black);
    if (options.enableTranspositionTable) {
        auto it = transpositionTable.find(key);
        if (it != transpositionTable.end() && it->second.hasStaticEval) {
            return it->second.blackWinProbability;
        }
    }

    double probability = Evaluation::evaluateBoard(board, Player::Black).winProbability;

    if (options.enableTranspositionTable) {
        auto& entry = transpositionTable[key];
        entry.hasStaticEval = true;
        entry.blackWinProbability = probability;
    }

    return probability;
}

std::optional<Move> MCTSSolver::getRolloutMove(const Board& board, Player side) {
    if (auto threat = Heuristics::getFastThreatMove(board, side)) {
        return threat;
    }

    Heuristics::CandidateOptions candidateOptions;
    candidateOptions.maxMoves = options.rolloutSampleSize;
    auto scoredMoves = Heuristics::getScoredCandidateMoves(board, side, candidateOptions);
    if (scoredMoves.empty()) return std::nullopt;
    if (scoredMoves[0].totalScore() >= 1000000) return scoredMoves[0].move;

    std::vector<double> weights;
    weights.reserve(scoredMoves.size());
    double totalWeight = 0.0;
    for (const auto& scored : scoredMoves) {
        double weight = std::exp(std::min(10.0, scored.totalScore() / 50000.0));
        weights.push_back(weight);
        totalWeight += weight;
    }

    std::uniform_real_distribution<> dis(0.0, totalWeight);
    double sample = dis(rng);
    double cumulative = 0.0;
    for (size_t i = 0; i < scoredMoves.size(); ++i) {
        cumulative += weights[i];
        if (sample <= cumulative) return scoredMoves[i].move;
    }

    return scoredMoves[0].move;
}

void MCTSSolver::step() {
    MCTSNode* node = root.get();
    Board tempBoard = rootBoard;

    // Selection - use progressive bias
    while (node->isFullyExpanded() && !node->children.empty()) {
        node = node->bestChild(1.41, true);  // UCT with progressive bias
        tempBoard.placeStone(node->move.row, node->move.col, node->move.player);
    }

    // Expansion - sorted candidate order gives tactical moves first.
    if (!node->isFullyExpanded() && !tempBoard.isFull()) {
        Move move = node->untriedMoves.front();
        node->untriedMoves.erase(node->untriedMoves.begin());

        Player currentPlayer =
            (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
        move.player = currentPlayer;

        // Calculate heuristic bias only for the chosen node (lightweight version)
        double heuristicBias = Heuristics::getHeuristicBias(tempBoard, move);
        int heuristicScore = 0;
        auto scoredMoves = Heuristics::getScoredCandidateMoves(tempBoard, currentPlayer);
        for (const auto& scored : scoredMoves) {
            if (scored.move.row == move.row && scored.move.col == move.col) {
                heuristicScore = scored.totalScore();
                break;
            }
        }

        tempBoard.placeStone(move.row, move.col, currentPlayer);
        auto child = std::make_unique<MCTSNode>(move, node, currentPlayer, heuristicBias,
                                                heuristicScore, tempBoard.getHash());

        Player nextPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
        child->untriedMoves = getOrderedMoves(tempBoard, nextPlayer, options.childBeamSize);

        node->children.push_back(std::move(child));
        node = node->children.back().get();
    }

    // Simulation (Rollout) with improved heuristic guidance
    Player rolloutPlayer = (node->playerJustMoved == Player::Black) ? Player::White : Player::Black;
    Board rolloutBoard = tempBoard;
    Player winner = Player::NoPlayer;

    int movesMade = 0;

    Player existingWinner = rolloutBoard.checkWinner();
    if (existingWinner != Player::NoPlayer) {
        winner = existingWinner;
    } else {
        while (movesMade < options.rolloutDepthLimit) {
            if (rolloutBoard.isFull()) {
                winner = Player::NoPlayer;
                break;
            }

            std::optional<Move> selectedMove = getRolloutMove(rolloutBoard, rolloutPlayer);
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

    double blackWinProbability = 0.5;
    if (winner == Player::Black) {
        blackWinProbability = 1.0;
    } else if (winner == Player::White) {
        blackWinProbability = 0.0;
    } else if (!rolloutBoard.isFull()) {
        blackWinProbability = evaluateStaticBlackWinProbability(rolloutBoard);
    }

    // Backpropagation
    while (node != nullptr) {
        node->visits++;
        double nodeWin = 0.5;
        if (node->playerJustMoved == Player::Black) {
            nodeWin = blackWinProbability;
        } else if (node->playerJustMoved == Player::White) {
            nodeWin = 1.0 - blackWinProbability;
        }
        node->wins += nodeWin;

        if (options.enableTranspositionTable) {
            uint64_t key = getTranspositionKey(node->stateHash, node->playerJustMoved);
            auto& entry = transpositionTable[key];
            entry.visits++;
            entry.wins += nodeWin;
        }
        node = node->parent;
    }

    totalSimulations++;
}

void MCTSSolver::run(int durationMs, std::function<void(double, int, double, double)> statusCb) {
    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = startTime;

    // Call callback immediately at start to show "thinking" status
    if (statusCb) {
        statusCb(50.0, 0, 0.0, 0.0);
    }

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= durationMs) break;

        step();

        auto updateDiff =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        // Update every 50ms for more responsive UI
        if (updateDiff >= 50 && statusCb) {
            double winRate = 0.0;
            MCTSNode* best = root->bestChild(0, false);  // Don't use progressive bias for display
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

void MCTSSolver::runIterations(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        step();
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
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime)
                            .count() /
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
        if (child->visits <= 0) continue;

        bool better = child->visits > maxVisits;
        if (!better && bestNode) {
            int closeVisitMargin = std::max(2, maxVisits / 20);
            if (std::abs(child->visits - maxVisits) <= closeVisitMargin) {
                double childValue = child->wins / child->visits + child->heuristicBias +
                                    child->heuristicScore / 10000000.0;
                double bestValue = bestNode->wins / bestNode->visits + bestNode->heuristicBias +
                                   bestNode->heuristicScore / 10000000.0;
                better = childValue > bestValue;
            }
        }

        if (better) {
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
            double heuristicRatio = std::min(1.0, child->heuristicScore / 10000000.0);
            eval.score = (float)(winRate * 0.6 + visitRatio * 0.3 + heuristicRatio * 0.1);
            evals.push_back(eval);
        }
    }

    return evals;
}
