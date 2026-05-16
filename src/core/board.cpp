#include "board.h"

#include <random>
#include <stdexcept>

// Static member initialization
uint64_t Board::zobristTable[15][15][2];
bool Board::zobristInitialized = false;

namespace {
int validateBoardSize(int size) {
    if (size < 1 || size > 15) {
        throw std::invalid_argument("Board size must be between 1 and 15");
    }
    return size;
}
}  // namespace

void Board::initZobrist() {
    if (zobristInitialized) return;

    std::mt19937_64 rng(0x5EED12345678ULL);  // Fixed seed for reproducibility
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            zobristTable[r][c][0] = rng();  // Black
            zobristTable[r][c][1] = rng();  // White
        }
    }
    zobristInitialized = true;
}

Board::Board(int size)
    : boardSize(validateBoardSize(size)),
      grid(boardSize * boardSize, Player::NoPlayer),
      stonesPlaced(0),
      currentHash(0) {
    initZobrist();
}

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
    if (player == Player::NoPlayer) {
        return false;
    }
    if (!isEmpty(row, col)) {
        return false;
    }
    grid[row * boardSize + col] = player;
    stonesPlaced++;

    // Update Zobrist hash
    int playerIndex = (player == Player::Black) ? 0 : 1;
    currentHash ^= zobristTable[row][col][playerIndex];

    return true;
}

void Board::removeStone(int row, int col) {
    if (isInside(row, col) && grid[row * boardSize + col] != Player::NoPlayer) {
        Player player = grid[row * boardSize + col];
        grid[row * boardSize + col] = Player::NoPlayer;
        stonesPlaced--;

        // Update Zobrist hash (XOR is its own inverse)
        int playerIndex = (player == Player::Black) ? 0 : 1;
        currentHash ^= zobristTable[row][col][playerIndex];
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
    return stonesPlaced >= boardSize * boardSize;
}

int Board::stoneCount() const {
    return stonesPlaced;
}

void Board::clear() {
    std::fill(grid.begin(), grid.end(), Player::NoPlayer);
    stonesPlaced = 0;
    currentHash = 0;
}

uint64_t Board::getHash() const {
    return currentHash;
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

std::vector<std::pair<int, int>> Board::getWinningLineInDirection(int row, int col, int dr,
                                                                  int dc) const {
    Player p = at(row, col);
    if (p == Player::NoPlayer) return {};

    std::vector<std::pair<int, int>> line;
    line.push_back({row, col});

    // Extend in positive direction
    for (int i = 1; i < 5; ++i) {
        int nr = row + i * dr;
        int nc = col + i * dc;
        if (at(nr, nc) == p) {
            line.push_back({nr, nc});
        } else {
            break;
        }
    }

    // Extend in negative direction
    for (int i = 1; i < 5; ++i) {
        int nr = row - i * dr;
        int nc = col - i * dc;
        if (at(nr, nc) == p) {
            line.push_back({nr, nc});
        } else {
            break;
        }
    }

    if (line.size() >= 5) return line;
    return {};
}

std::vector<std::pair<int, int>> Board::getWinningLine(const Move& lastMove) const {
    int r = lastMove.row;
    int c = lastMove.col;

    // Check all four directions
    std::vector<std::pair<int, int>> line;

    line = getWinningLineInDirection(r, c, 0, 1);  // Horizontal
    if (!line.empty()) return line;

    line = getWinningLineInDirection(r, c, 1, 0);  // Vertical
    if (!line.empty()) return line;

    line = getWinningLineInDirection(r, c, 1, 1);  // Diagonal
    if (!line.empty()) return line;

    line = getWinningLineInDirection(r, c, 1, -1);  // Anti-diagonal
    if (!line.empty()) return line;

    return {};
}
