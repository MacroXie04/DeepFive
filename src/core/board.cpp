#include "board.h"

Board::Board(int size) : boardSize(size), grid(size * size, Player::NoPlayer), stoneCount(0) {}

int Board::size() const {
    return boardSize;
}

Player Board::at(int row, int col) const {
    if (!isInside(row, col)) {
        return Player::NoPlayer;
    }
    return grid[row * boardSize + col];
}

bool Board::isInside(int row, int col) const {
    return row >= 0 && row < boardSize && col >= 0 && col < boardSize;
}

bool Board::isEmpty(int row, int col) const {
    return isInside(row, col) && grid[row * boardSize + col] == Player::NoPlayer;
}

bool Board::placeStone(int row, int col, Player player) {
    if (!isEmpty(row, col)) {
        return false;
    }
    grid[row * boardSize + col] = player;
    stoneCount++;
    return true;
}

void Board::removeStone(int row, int col) {
    if (isInside(row, col) && grid[row * boardSize + col] != Player::NoPlayer) {
        grid[row * boardSize + col] = Player::NoPlayer;
        stoneCount--;
    }
}

Player Board::checkWinner() const {
    for (int r = 0; r < boardSize; ++r) {
        for (int c = 0; c < boardSize; ++c) {
            Player p = at(r, c);
            if (p == Player::NoPlayer) continue;

            if (checkDirection(r, c, 0, 1) == p) return p;
            if (checkDirection(r, c, 1, 0) == p) return p;
            if (checkDirection(r, c, 1, 1) == p) return p;
            if (checkDirection(r, c, 1, -1) == p) return p;
        }
    }
    return Player::NoPlayer;
}

Player Board::checkWinner(const Move& lastMove) const {
    Player p = lastMove.player;
    int r = lastMove.row;
    int c = lastMove.col;

    if (checkDirection(r, c, 0, 1) == p) return p;
    if (checkDirection(r, c, 1, 0) == p) return p;
    if (checkDirection(r, c, 1, 1) == p) return p;
    if (checkDirection(r, c, 1, -1) == p) return p;

    return Player::NoPlayer;
}

bool Board::isFull() const {
    return stoneCount >= boardSize * boardSize;
}

void Board::clear() {
    std::fill(grid.begin(), grid.end(), Player::NoPlayer);
    stoneCount = 0;
}

Player Board::checkDirection(int row, int col, int dr, int dc) const {
    Player p = at(row, col);
    if (p == Player::NoPlayer) return Player::NoPlayer;

    int count = 1;

    for (int i = 1; i < 5; ++i) {
        if (at(row + i * dr, col + i * dc) == p) {
            count++;
        } else {
            break;
        }
    }

    for (int i = 1; i < 5; ++i) {
        if (at(row - i * dr, col - i * dc) == p) {
            count++;
        } else {
            break;
        }
    }

    if (count >= 5) return p;
    return Player::NoPlayer;
}
