#include "bot.h"

#include <FL/Fl.H>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <thread>

#include "../search/ForcedBFS.h"
#include "../search/ForcedDFS.h"
#include "../search/PathReconstruction.h"
#include "heuristics.h"
#include "mcts.h"

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
        case BotMode::Instant: return "Instant";
        case BotMode::Auto: return "Auto";
        case BotMode::Thinking: return "Thinking";
        case BotMode::Pro: return "Pro";
        default: return "Unknown";
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

std::optional<Move> GomokuBot::chooseMove(const Board& board, Player side) {
    if (board.isFull()) return std::nullopt;

    Player opp = (side == Player::Black) ? Player::White : Player::Black;

    // 0. First move: play center (Tengen) immediately
    int center = board.size() / 2;
    if (board.isEmpty(center, center)) {
        bool isEmpty = true;
        for (int r = 0; r < board.size() && isEmpty; ++r) {
            for (int c = 0; c < board.size() && isEmpty; ++c) {
                if (!board.isEmpty(r, c)) isEmpty = false;
            }
        }
        if (isEmpty) {
            lastAlgorithmStage = AlgorithmStage::Tengen;
            return Move{center, center, side};
        }
    }

    // 1. Can we win directly (Five)?
    for (const auto& mv : Heuristics::getCandidateMoves(board, side)) {
        Board temp = board;
        temp.placeStone(mv.row, mv.col, side);
        if (temp.checkWinner() == side) {
            lastAlgorithmStage = AlgorithmStage::DirectWin;
            return mv;
        }
    }

    // 2. Must block opponent's win?
    for (const auto& mv : Heuristics::getCandidateMoves(board, side)) {
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
    if (ForcedNode* forcedWin = BFS_FindWin(board, side, vcfDepth)) {
        auto path = reconstructPath(forcedWin);
        if (!path.empty()) {
            lastAlgorithmStage = AlgorithmStage::VCF;
            return path.front();
        }
    }

    // 6. Block opponent's VCF
    if (ForcedNode* forcedLose = BFS_FindLose(board, side, vcfBlockDepth)) {
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
        if (ForcedNode* vctWin = VCT_Solve(board, side, vctDepth)) {
            auto path = reconstructPath(vctWin);
            if (!path.empty()) {
                lastAlgorithmStage = AlgorithmStage::VCT;
                return path.front();
            }
        }
    }

    // 8. Use MCTS for position evaluation
    lastAlgorithmStage = AlgorithmStage::MCTS;
    auto scoredMoves = Heuristics::getScoredMoves(board, side);
    
    int durationMs = 2000;  // Default

    switch (currentMode) {
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
            int stoneCount = 0;
            int size = board.size();
            for (int r = 0; r < size; ++r)
                for (int c = 0; c < size; ++c)
                    if (!board.isEmpty(r, c)) stoneCount++;

            if (stoneCount < 10) {
                durationMs = 200;
            } else if (stoneCount < 30) {
                durationMs = 2000;
            } else {
                durationMs = 10000;
            }
            break;
        }
    }

    if (selfPlayMode) {
        durationMs = (int)(durationMs * 1.5);
    }

    MCTSSolver solver(board, side);

    // Wrap callback to ensure UI updates if needed (though chooseMove is usually blocking)
    // But statusCallback might expect to be called on main thread if it touches UI.
    // Since chooseMove blocks main thread (usually), calling callback directly is fine.
    // However, if we want to process events, we should call Fl::check().

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
        
        Fl::check();
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

    analysisThread = std::thread([this, board, side, cb]() {
        MCTSSolver solver(board, side);

        // Callback wrapper for thread safety
        auto safeCb = [cb](double wr, int sims, double t) {
            struct UpdateData {
                std::function<void(double, int, double)> cb;
                double wr;
                int sims;
                double t;
            };
            auto* data = new UpdateData{cb, wr, sims, t};

            Fl::awake(
                [](void* d) {
                    auto* data = (UpdateData*)d;
                    if (data->cb) data->cb(data->wr, data->sims, data->t);
                    delete data;
                },
                data);
        };

        solver.runContinuous(stopAnalysisFlag, safeCb);
    });
}
