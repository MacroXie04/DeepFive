#ifndef BOARD_H
#define BOARD_H

#include <vector>

enum class Player {
    None,
    Black,
    White
};

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
    bool isFull() const;
    void clear();

private:
    int boardSize;
    std::vector<Player> grid;
    int stoneCount;

    Player checkDirection(int row, int col, int dr, int dc) const;
};

#endif

