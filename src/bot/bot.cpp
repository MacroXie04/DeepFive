#include "bot.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <utility>

#include "../search/ForcedBFS.h"
#include "../search/ForcedDFS.h"
#include "../search/PathReconstruction.h"
#include "heuristics.h"
#include "mcts.h"

namespace {
Player opponentOf(Player player) {
    return (player == Player::Black) ? Player::White : Player::Black;
}

std::optional<Move> findOpeningBookMove(const Board& board, Player side) {
    int size = board.size();
    int center = size / 2;

    if (board.stoneCount() == 0 && board.isEmpty(center, center)) {
        return Move{center, center, side};
    }

    if (board.stoneCount() > 2) return std::nullopt;

    const int offsets[][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {0, -1}, {-1, 0}, {0, 1}, {1, 0}};
    for (const auto& offset : offsets) {
        int row = center + offset[0];
        int col = center + offset[1];
        if (board.isEmpty(row, col)) {
            return Move{row, col, side};
        }
    }

    return std::nullopt;
}

int calculateSearchDurationMs(const Board& board, Player side, BotMode mode, bool selfPlayMode) {
    int durationMs = 2000;

    switch (mode) {
        case BotMode::Instant:
            durationMs = 200;
            break;
        case BotMode::Thinking:
            durationMs = 5000;
            break;
        case BotMode::Pro:
            durationMs = 15000;
            break;
        case BotMode::Auto: {
            int stoneCount = board.stoneCount();
            if (stoneCount < 10) {
                durationMs = 200;
            } else if (stoneCount < 30) {
                durationMs = 2000;
            } else {
                durationMs = 6000;
            }

            Heuristics::CandidateOptions options;
            options.maxMoves = 40;
            auto candidates = Heuristics::getScoredCandidateMoves(board, side, options);
            if (!candidates.empty()) {
                int best = candidates[0].totalScore();
                if (best >= 1000000) {
                    durationMs = std::max(durationMs, 3000);
                } else if ((int)candidates.size() > 24) {
                    durationMs += 1000;
                }

                if (candidates.size() >= 2) {
                    int second = candidates[1].totalScore();
                    if (std::abs(best - second) < 50000) {
                        durationMs += 1500;
                    }
                }
            }

            durationMs = std::min(durationMs, 10000);
            break;
        }
    }

    if (selfPlayMode) {
        durationMs = (int)(durationMs * 1.5);
    }
    return durationMs;
}
}  // namespace

GomokuBot::GomokuBot() : currentMode(BotMode::Auto) {}

GomokuBot::~GomokuBot() {
    stopAnalysis();
}

void GomokuBot::setMode(BotMode mode) {
    currentMode = mode;
}

BotMode GomokuBot::getMode() const {
    return currentMode;
}

const char* GomokuBot::getModeName() const {
    switch (currentMode) {
        case BotMode::Instant:
            return "Instant";
        case BotMode::Auto:
            return "Auto";
        case BotMode::Thinking:
            return "Thinking";
        case BotMode::Pro:
            return "Pro";
        default:
            return "Unknown";
    }
}

void GomokuBot::setSelfPlayMode(bool enabled) {
    selfPlayMode = enabled;
}

bool GomokuBot::isSelfPlayMode() const {
    return selfPlayMode;
}

void GomokuBot::setSearchCallback(std::function<void(double, int, double, double)> cb) {
    statusCallback = cb;
}

void GomokuBot::setCandidateCallback(CandidateCallback cb) {
    candidateCallback = cb;
}

void GomokuBot::setEventPump(EventPump pump) {
    eventPump = std::move(pump);
}

void GomokuBot::setUiDispatcher(UiDispatcher dispatcher) {
    uiDispatcher = std::move(dispatcher);
}

void GomokuBot::setMCTSOptions(const MCTSSolver::Options& options) {
    mctsOptions = options;
}

void GomokuBot::setRandomSeed(uint32_t seed) {
    mctsOptions.seed = seed;
    Heuristics::setRandomSeed(seed);
}

std::optional<Move> GomokuBot::chooseMove(const Board& board, Player side) {
    if (board.isFull()) return std::nullopt;

    Player opp = opponentOf(side);

    // 0. Opening book: center, then symmetric center-adjacent replies.
    if (auto openingMove = findOpeningBookMove(board, side)) {
        lastAlgorithmStage = AlgorithmStage::Tengen;
        return openingMove;
    }

    // 1. Can we win directly (Five)?
    auto candidates = Heuristics::getCandidateMoves(board, side);
    for (const auto& mv : candidates) {
        Board temp = board;
        temp.placeStone(mv.row, mv.col, side);
        if (temp.checkWinner() == side) {
            lastAlgorithmStage = AlgorithmStage::DirectWin;
            return mv;
        }
    }

    // 2. Must block opponent's win?
    for (const auto& mv : candidates) {
        Board temp = board;
        temp.placeStone(mv.row, mv.col, opp);
        if (temp.checkWinner() == opp) {
            lastAlgorithmStage = AlgorithmStage::BlockWin;
            return mv;
        }
    }

    // 3. Can we create a double threat (活四, 双活三, 冲四活三)?
    if (auto doubleThreat = Heuristics::findDoubleThreat(board, side)) {
        lastAlgorithmStage = AlgorithmStage::DoubleThreat;
        return doubleThreat;
    }

    // 4. Must block opponent's double threat?
    if (auto oppDoubleThreat = Heuristics::findDoubleThreat(board, opp)) {
        lastAlgorithmStage = AlgorithmStage::BlockThreat;
        return Move{oppDoubleThreat->row, oppDoubleThreat->col, side};
    }

    // VCF/VCT search depth depends on Tournament mode
    int vcfDepth = selfPlayMode ? 35 : 25;
    int vcfBlockDepth = selfPlayMode ? 25 : 17;
    int vctDepth = selfPlayMode ? 15 : 10;

    // 5. Search forced wins (VCF - continuous fours)
    if (ForcedNode* forcedWin = BFS_FindWin(board, side, vcfDepth, eventPump)) {
        auto path = reconstructPath(forcedWin);
        if (!path.empty()) {
            lastAlgorithmStage = AlgorithmStage::VCF;
            return path.front();
        }
    }

    // 6. Block opponent's VCF
    if (ForcedNode* forcedLose = BFS_FindLose(board, side, vcfBlockDepth, eventPump)) {
        auto path = reconstructPath(forcedLose);
        if (!path.empty()) {
            lastAlgorithmStage = AlgorithmStage::BlockVCF;
            Move m = path.front();
            m.player = side;
            return m;
        }
    }

    // 7. Search VCT (continuous threes) - only in Tournament mode for speed
    if (selfPlayMode) {
        if (ForcedNode* vctWin = VCT_Solve(board, side, vctDepth, eventPump)) {
            auto path = reconstructPath(vctWin);
            if (!path.empty()) {
                lastAlgorithmStage = AlgorithmStage::VCT;
                return path.front();
            }
        }
    }

    // 8. Use MCTS for position evaluation
    lastAlgorithmStage = AlgorithmStage::MCTS;

    int durationMs = calculateSearchDurationMs(board, side, currentMode, selfPlayMode);
    MCTSSolver solver(board, side, mctsOptions);

    auto cb = [this, &solver, side](double wr, int sims, double t, double p) {
        if (statusCallback) statusCallback(wr, sims, t, p);

        // Send candidate evaluations for visualization (always, not just self-play)
        if (candidateCallback && sims > 50) {
            // Only update visualization after sufficient simulations
            auto evals = solver.getCandidateEvaluations();
            if (!evals.empty()) {
                std::vector<std::tuple<int, int, Player, float>> candidates;
                for (const auto& e : evals) {
                    candidates.emplace_back(e.move.row, e.move.col, side, e.score);
                }
                candidateCallback(candidates);
            }
        }

        if (eventPump) eventPump();
    };

    solver.run(durationMs, cb);

    // Clear visualization after thinking is done
    if (candidateCallback) {
        candidateCallback({});
    }

    auto result = solver.getBestMove();
    return result.first;
}

int GomokuBot::runBenchmark(int durationMs) {
    // Empty-ish board for consistent benchmark
    Board board(15);
    board.placeStone(7, 7, Player::Black);

    MCTSSolver solver(board, Player::White);
    solver.run(durationMs);

    auto result = solver.getBestMove();
    int sims = result.second;

    // SPS = sims / (duration / 1000)
    return (int)((double)sims / ((double)durationMs / 1000.0));
}

void GomokuBot::stopAnalysis() {
    stopAnalysisFlag = true;
    if (analysisThread.joinable()) {
        analysisThread.join();
    }
    analysisRunning = false;
}

void GomokuBot::startAnalysis(const Board& board, Player side,
                              std::function<void(double, int, double)> cb) {
    stopAnalysis();  // Stop any existing analysis
    stopAnalysisFlag = false;
    analysisRunning = true;
    auto dispatcher = uiDispatcher;

    analysisThread = std::thread([this, board, side, cb, dispatcher]() {
        MCTSSolver solver(board, side);

        // Callback wrapper for thread safety
        auto safeCb = [cb, dispatcher](double wr, int sims, double t) {
            if (dispatcher) {
                dispatcher([cb, wr, sims, t]() {
                    if (cb) cb(wr, sims, t);
                });
            } else if (cb) {
                cb(wr, sims, t);
            }
        };

        solver.runContinuous(stopAnalysisFlag, safeCb);
    });
}
