#include "game.h"

GomokuGame::GomokuGame(int boardSize)
    : board(boardSize),
      mode(GameMode::HumanVsBot),
      state(GameState::Playing),
      currentPlayer(Player::Black),
      winner(Player::NoPlayer),
      botSide(Player::White) {}

void GomokuGame::setMode(GameMode m) {
    mode = m;
    reset();
}

void GomokuGame::setBotSide(Player side) {
    botSide = side;
    reset();
}

const Board& GomokuGame::getBoard() const {
    return board;
}

GameState GomokuGame::getState() const {
    return state;
}

Player GomokuGame::getCurrentPlayer() const {
    return currentPlayer;
}

Player GomokuGame::getWinner() const {
    return winner;
}

bool GomokuGame::isBotTurn() const {
    if (state != GameState::Playing) return false;
    if (mode == GameMode::BotVsBot) return true;
    return (mode == GameMode::HumanVsBot && currentPlayer == botSide);
}

bool GomokuGame::canPlayAt(int row, int col) const {
    return state == GameState::Playing && board.isEmpty(row, col);
}

bool GomokuGame::playHumanMove(int row, int col) {
    if (!canPlayAt(row, col)) return false;
    if (isBotTurn()) return false;

    if (board.placeStone(row, col, currentPlayer)) {
        history.push_back({row, col, currentPlayer});
        redoStack.clear();  // Clear redo stack on new move
        checkGameStatus();
        if (state == GameState::Playing) {
            switchTurn();
        }
        return true;
    }
    return false;
}

bool GomokuGame::playBotMove(GomokuBot& bot) {
    if (!isBotTurn()) return false;

    std::optional<Move> move = bot.chooseMove(board, currentPlayer);
    if (move.has_value()) {
        if (board.placeStone(move->row, move->col, currentPlayer)) {
            history.push_back({move->row, move->col, currentPlayer});
            redoStack.clear();  // Clear redo stack on new move
            checkGameStatus();
            if (state == GameState::Playing) {
                switchTurn();
            }
            return true;
        }
    }
    return false;
}

bool GomokuGame::canUndo() const {
    return !history.empty();  // Allow undo even after game finished
}

void GomokuGame::undoLastMove() {
    if (history.empty()) return;

    Move last = history.back();
    history.pop_back();
    redoStack.push_back(last);  // Push to redo stack
    board.removeStone(last.row, last.col);

    currentPlayer = last.player;
    state = GameState::Playing;
    winner = Player::NoPlayer;
    winningLine.clear();
}

bool GomokuGame::canRedo() const {
    return !redoStack.empty() && state == GameState::Playing;
}

void GomokuGame::redo() {
    if (redoStack.empty()) return;

    Move move = redoStack.back();
    redoStack.pop_back();
    
    board.placeStone(move.row, move.col, move.player);
    history.push_back(move);
    
    checkGameStatus();
    if (state == GameState::Playing) {
        switchTurn();
    }
}

void GomokuGame::reset() {
    board.clear();
    history.clear();
    redoStack.clear();
    state = GameState::Playing;
    winner = Player::NoPlayer;
    currentPlayer = Player::Black;
    winningLine.clear();
}

const std::vector<Move>& GomokuGame::getHistory() const {
    return history;
}

const std::vector<std::pair<int, int>>& GomokuGame::getWinningLine() const {
    return winningLine;
}

void GomokuGame::forceWin(Player winnerPlayer) {
    winner = winnerPlayer;
    state = GameState::Finished;
}

void GomokuGame::checkGameStatus() {
    if (history.empty()) return;

    Move last = history.back();
    Player p = board.checkWinner(last);

    if (p != Player::NoPlayer) {
        winner = p;
        state = GameState::Finished;
        winningLine = board.getWinningLine(last);
    } else if (board.isFull()) {
        winner = Player::NoPlayer;
        state = GameState::Finished;
    }
}

void GomokuGame::switchTurn() {
    currentPlayer = (currentPlayer == Player::Black) ? Player::White : Player::Black;
}
