#include "bot.h"
#include <vector>
#include <algorithm>
#include <random>
#include <limits>

GomokuBot::GomokuBot(BotDifficulty difficulty) : difficulty(difficulty) {}

void GomokuBot::setDifficulty(BotDifficulty difficulty) {
    this->difficulty = difficulty;
}

BotDifficulty GomokuBot::getDifficulty() const {
    return difficulty;
}

std::optional<Move> GomokuBot::chooseMove(const Board& board, Player side) {
    if (board.isFull()) return std::nullopt;

    switch (difficulty) {
        case BotDifficulty::Easy:
            return chooseEasyMove(board, side);
        case BotDifficulty::Normal:
            return chooseNormalMove(board, side);
        case BotDifficulty::Hard:
            return chooseHardMove(board, side);
    }
    return chooseEasyMove(board, side);
}

std::optional<Move> GomokuBot::chooseEasyMove(const Board& board, Player side) {
    std::vector<Move> candidates = getCandidateMoves(board, side);
    if (candidates.empty()) return std::nullopt;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, candidates.size() - 1);

    return candidates[dis(gen)];
}

std::optional<Move> GomokuBot::chooseNormalMove(const Board& board, Player side) {
    std::vector<Move> candidates = getCandidateMoves(board, side);
    if (candidates.empty()) return std::nullopt;

    Move bestMove = candidates[0];
    long long bestScore = std::numeric_limits<long long>::min();

    Board tempBoard = board;
    
    for (const auto& move : candidates) {
        tempBoard.placeStone(move.row, move.col, side);
        long long score = evaluateBoard(tempBoard, side);
        
        Player opp = (side == Player::Black) ? Player::White : Player::Black;
        long long oppScore = evaluateBoard(tempBoard, opp);
        
        long long finalScore = score - oppScore;
        
        if (finalScore > bestScore) {
            bestScore = finalScore;
            bestMove = move;
        }
        
        tempBoard = board; 
    }

    return bestMove;
}

std::optional<Move> GomokuBot::chooseHardMove(const Board& board, Player side) {
    std::vector<Move> candidates = getCandidateMoves(board, side);
    if (candidates.empty()) {
        int center = board.size() / 2;
        if (board.isEmpty(center, center)) {
            return Move{center, center, side};
        }
        return std::nullopt;
    }

    Move bestMove = candidates[0];
    int bestVal = std::numeric_limits<int>::min();
    
    int depth = 2;
    int alpha = std::numeric_limits<int>::min();
    int beta = std::numeric_limits<int>::max();

    Board tempBoard = board;

    for (const auto& move : candidates) {
        tempBoard.placeStone(move.row, move.col, side);
        
        int val = minimax(tempBoard, depth - 1, false, alpha, beta, side);
        
        if (val > bestVal) {
            bestVal = val;
            bestMove = move;
        }
        
        alpha = std::max(alpha, bestVal);
        tempBoard = board;
    }

    return bestMove;
}

int GomokuBot::minimax(Board& board, int depth, bool isMax, int alpha, int beta, Player side) {
    Player winner = board.checkWinner();
    if (winner == side) return 1000000;
    if (winner != Player::None) return -1000000;
    if (board.isFull()) return 0;
    if (depth == 0) {
        long long eval = evaluateBoard(board, side);
        Player opp = (side == Player::Black) ? Player::White : Player::Black;
        long long evalOpp = evaluateBoard(board, opp);
        return (int)(eval - evalOpp);
    }

    Player currentSide = isMax ? side : ((side == Player::Black) ? Player::White : Player::Black);
    std::vector<Move> candidates = getCandidateMoves(board, currentSide);

    if (isMax) {
        int maxEval = std::numeric_limits<int>::min();
        for (const auto& move : candidates) {
            Board nextBoard = board;
            nextBoard.placeStone(move.row, move.col, currentSide);
            int eval = minimax(nextBoard, depth - 1, false, alpha, beta, side);
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = std::numeric_limits<int>::max();
        for (const auto& move : candidates) {
            Board nextBoard = board;
            nextBoard.placeStone(move.row, move.col, currentSide);
            int eval = minimax(nextBoard, depth - 1, true, alpha, beta, side);
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) break;
        }
        return minEval;
    }
}

std::vector<Move> GomokuBot::getCandidateMoves(const Board& board, Player side) {
    std::vector<Move> moves;
    int size = board.size();
    std::vector<bool> visited(size * size, false);
    
    bool emptyBoard = true;

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (!board.isEmpty(r, c)) {
                emptyBoard = false;
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

long long GomokuBot::evaluateBoard(const Board& board, Player side) {
    long long score = 0;
    int size = board.size();
    
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (board.at(r, c) != side) continue;
            
            for (auto& dir : directions) {
                int prevR = r - dir[0];
                int prevC = c - dir[1];
                if (board.isInside(prevR, prevC) && board.at(prevR, prevC) == side) continue;
                
                int count = 0;
                int openEnds = 0;
                
                if (board.isInside(prevR, prevC) && board.isEmpty(prevR, prevC)) openEnds++;
                
                int currR = r;
                int currC = c;
                while (board.isInside(currR, currC) && board.at(currR, currC) == side) {
                    count++;
                    currR += dir[0];
                    currC += dir[1];
                }
                
                if (board.isInside(currR, currC) && board.isEmpty(currR, currC)) openEnds++;
                
                score += evaluateLine(count, openEnds, true);
            }
        }
    }
    
    return score;
}

long long GomokuBot::evaluateLine(int count, int openEnds, bool currentTurn) {
    if (count >= 5) return 100000;
    if (count == 4) {
        if (openEnds == 2) return 10000;
        if (openEnds == 1) return 1000;
    }
    if (count == 3) {
        if (openEnds == 2) return 1000;
        if (openEnds == 1) return 100;
    }
    if (count == 2) {
        if (openEnds == 2) return 100;
        if (openEnds == 1) return 10;
    }
    return 0;
}
