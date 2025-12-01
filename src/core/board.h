#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <utility>
#include <vector>

enum class Player { NoPlayer, Black, White };

struct Move {
    int row;
    int col;
    Player player;
};

class Board {
   public:
    explicit Board(int size = 15);

    int size() const;
    Player at(int row, int col) const;
    bool isInside(int row, int col) const;
    bool isEmpty(int row, int col) const;
    bool placeStone(int row, int col, Player player);
    void removeStone(int row, int col);
    Player checkWinner() const;
    Player checkWinner(const Move& lastMove) const;
    std::vector<std::pair<int, int>> getWinningLine(const Move& lastMove) const;
    bool isFull() const;
    void clear();
    uint64_t getHash() const;

   private:
    int boardSize;
    std::vector<Player> grid;
    int stoneCount;
    uint64_t currentHash;

    // Zobrist hashing
    static uint64_t zobristTable[15][15][2];
    static bool zobristInitialized;
    static void initZobrist();

    Player checkDirection(int row, int col, int dr, int dc) const;
    std::vector<std::pair<int, int>> getWinningLineInDirection(int row, int col, int dr,
                                                               int dc) const;
};

#endif
