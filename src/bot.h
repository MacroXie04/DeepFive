#ifndef BOT_H
#define BOT_H

#include "board.h"
#include <optional>

enum class BotDifficulty {
    Easy,
    Normal,
    Hard
};

class GomokuBot {
public:
    explicit GomokuBot(BotDifficulty difficulty = BotDifficulty::Normal);
    
    void setDifficulty(BotDifficulty difficulty);
    BotDifficulty getDifficulty() const;
    std::optional<Move> chooseMove(const Board& board, Player side);

private:
    BotDifficulty difficulty;

    std::optional<Move> chooseEasyMove(const Board& board, Player side);
    std::optional<Move> chooseNormalMove(const Board& board, Player side);
    std::optional<Move> chooseHardMove(const Board& board, Player side);

    long long evaluateBoard(const Board& board, Player side);
    long long evaluateLine(int count, int openEnds, bool currentTurn);
    
    int minimax(Board& board, int depth, bool isMax, int alpha, int beta, Player side);
    std::vector<Move> getCandidateMoves(const Board& board, Player side);
};

#endif

