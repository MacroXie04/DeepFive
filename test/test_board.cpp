#include "test_utils.h"
#include "../src/core/board.h"

// Helper to print Player enum
static std::ostream& operator<<(std::ostream& os, Player p) {
    switch (p) {
        case Player::NoPlayer: os << "NoPlayer"; break;
        case Player::Black: os << "Black"; break;
        case Player::White: os << "White"; break;
    }
    return os;
}

TEST_CASE(TestBoardInitialization) {
    Board board(15);
    ASSERT_EQ(15, board.size());
    
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            ASSERT_EQ(Player::NoPlayer, board.at(r, c));
            ASSERT_TRUE(board.isEmpty(r, c));
        }
    }
    ASSERT_FALSE(board.isFull());
}

TEST_CASE(TestPlaceStone) {
    Board board(15);
    
    // Place valid stone
    bool success = board.placeStone(7, 7, Player::Black);
    ASSERT_TRUE(success);
    ASSERT_EQ(Player::Black, board.at(7, 7));
    ASSERT_FALSE(board.isEmpty(7, 7));
    
    // Place invalid stone (occupied)
    success = board.placeStone(7, 7, Player::White);
    ASSERT_FALSE(success);
    ASSERT_EQ(Player::Black, board.at(7, 7)); // Should remain Black
    
    // Place invalid stone (out of bounds)
    ASSERT_FALSE(board.placeStone(-1, 0, Player::White));
    ASSERT_FALSE(board.placeStone(15, 0, Player::White));
}

TEST_CASE(TestWinHorizontal) {
    Board board(15);
    // Black places 5 in a row horizontally
    board.placeStone(7, 5, Player::Black);
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 7, Player::Black);
    board.placeStone(7, 8, Player::Black);
    board.placeStone(7, 9, Player::Black); // 5th stone
    
    ASSERT_EQ(Player::Black, board.checkWinner());
}

TEST_CASE(TestWinVertical) {
    Board board(15);
    // White places 5 in a row vertically
    board.placeStone(5, 7, Player::White);
    board.placeStone(6, 7, Player::White);
    board.placeStone(7, 7, Player::White);
    board.placeStone(8, 7, Player::White);
    board.placeStone(9, 7, Player::White);
    
    ASSERT_EQ(Player::White, board.checkWinner());
}

TEST_CASE(TestWinDiagonal) {
    Board board(15);
    // Black places 5 in a row diagonal (\)
    board.placeStone(5, 5, Player::Black);
    board.placeStone(6, 6, Player::Black);
    board.placeStone(7, 7, Player::Black);
    board.placeStone(8, 8, Player::Black);
    board.placeStone(9, 9, Player::Black);
    
    ASSERT_EQ(Player::Black, board.checkWinner());
}

TEST_CASE(TestWinAntiDiagonal) {
    Board board(15);
    // White places 5 in a row anti-diagonal (/)
    board.placeStone(5, 9, Player::White);
    board.placeStone(6, 8, Player::White);
    board.placeStone(7, 7, Player::White);
    board.placeStone(8, 6, Player::White);
    board.placeStone(9, 5, Player::White);
    
    ASSERT_EQ(Player::White, board.checkWinner());
}

TEST_CASE(TestClearBoard) {
    Board board(15);
    board.placeStone(7, 7, Player::Black);
    ASSERT_FALSE(board.isEmpty(7, 7));
    
    board.clear();
    ASSERT_TRUE(board.isEmpty(7, 7));
    ASSERT_EQ(Player::NoPlayer, board.checkWinner());
}

