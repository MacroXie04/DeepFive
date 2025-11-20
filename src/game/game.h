#ifndef GAME_H
#define GAME_H

#include "../core/board.h"
#include "../ai/bot.h"
#include <vector>
#include <optional>

enum class GameMode {
    HumanVsBot,
    HumanVsHuman
};

enum class GameState {
    Playing,
    Finished
};

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
    
    bool playHumanMove(int row, int col);
    bool playBotMove(GomokuBot& bot);
    
    bool canUndo() const;
    void undoLastMove();
    void reset();

private:
    Board board;
    GameMode mode;
    GameState state;
    Player currentPlayer;
    Player winner;
    Player botSide;
    std::vector<Move> history;

    void checkGameStatus();
    void switchTurn();
};

#endif

