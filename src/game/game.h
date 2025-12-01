#ifndef GAME_H
#define GAME_H

#include <optional>
#include <utility>
#include <vector>

#include "../bot/bot.h"
#include "../core/board.h"

enum class GameMode { HumanVsBot, HumanVsHuman, BotVsBot };

enum class GameState { Playing, Finished };

class GomokuGame {
   public:
    explicit GomokuGame(int boardSize = 15);

    void setMode(GameMode m);
    void setBotSide(Player side);

    const Board& getBoard() const;
    GameState getState() const;
    Player getCurrentPlayer() const;
    Player getWinner() const;
    bool isBotTurn() const;
    bool canPlayAt(int row, int col) const;
    const std::vector<std::pair<int, int>>& getWinningLine() const;

    bool playHumanMove(int row, int col);
    bool playBotMove(GomokuBot& bot);

    bool canUndo() const;
    void undoLastMove();
    bool canRedo() const;
    void redo();
    void reset();

    const std::vector<Move>& getHistory() const;
    void forceWin(Player winnerPlayer);

   private:
    Board board;
    GameMode mode;
    GameState state;
    Player currentPlayer;
    Player winner;
    Player botSide;
    std::vector<Move> history;
    std::vector<Move> redoStack;
    std::vector<std::pair<int, int>> winningLine;

    void checkGameStatus();
    void switchTurn();
};

#endif
