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
    if (auto threatMove = Heuristics::checkImmediateThreats(board, side)) {
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

    int durationMs = 2000;  // Default Thinking

    switch (currentMode) {
        case BotMode::Instant:
            durationMs = 200;
            break;
        case BotMode::Thinking:
            durationMs = 5000;
            break;
        case BotMode::ExtendedThinking:
            durationMs = 10000;
            break;
        case BotMode::Pro:
            durationMs = 15000;
            break;
        case BotMode::Auto: {
            // Dynamic logic
            int stoneCount = 0;
            int size = board.size();
            for (int r = 0; r < size; ++r)
                for (int c = 0; c < size; ++c)
                    if (!board.isEmpty(r, c)) stoneCount++;

            if (stoneCount < 10) {
                durationMs = 200;  // Early game fast
            } else if (stoneCount < 30) {
                durationMs = 2000;  // Mid game normal
            } else {
                durationMs = 10000;  // Late game think harder
            }
            break;
        }
    }

    MCTSSolver solver(board, side);

    // Wrap callback to ensure UI updates if needed (though chooseMove is usually blocking)
    // But statusCallback might expect to be called on main thread if it touches UI.
    // Since chooseMove blocks main thread (usually), calling callback directly is fine.
    // However, if we want to process events, we should call Fl::check().

    auto cb = [this](double wr, int sims, double t, double p) {
        if (statusCallback) statusCallback(wr, sims, t, p);
        Fl::check();
    };

    solver.run(durationMs, cb);

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
