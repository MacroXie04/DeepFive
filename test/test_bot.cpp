#include "../src/bot/bot.h"
#include "../src/core/board.h"
#include "test_utils.h"

// Helper to print Player enum
static std::ostream& operator<<(std::ostream& os, Player p) {
    switch (p) {
        case Player::NoPlayer:
            os << "NoPlayer";
            break;
        case Player::Black:
            os << "Black";
            break;
        case Player::White:
            os << "White";
            break;
    }
    return os;
}

TEST_CASE(TestBotInitialization) {
    GomokuBot bot;
    // Default mode check if applicable, or just ensure no crash
    bot.setMode(BotMode::Instant);
    ASSERT_EQ((int)BotMode::Instant, (int)bot.getMode());
}

TEST_CASE(TestBotChooseMoveEmptyBoard) {
    GomokuBot bot;
    Board board(15);
    bot.setMode(BotMode::Instant);  // Use instant for fast tests

    auto moveOpt = bot.chooseMove(board, Player::Black);
    ASSERT_TRUE(moveOpt.has_value());
    Move move = moveOpt.value();

    ASSERT_TRUE(board.isInside(move.row, move.col));
    ASSERT_EQ(Player::Black, move.player);
}

TEST_CASE(TestBotChooseMoveNonEmptyBoard) {
    GomokuBot bot;
    Board board(15);
    board.placeStone(7, 7, Player::Black);
    bot.setMode(BotMode::Instant);

    // White to move
    auto moveOpt = bot.chooseMove(board, Player::White);
    ASSERT_TRUE(moveOpt.has_value());
    Move move = moveOpt.value();

    ASSERT_TRUE(board.isInside(move.row, move.col));
    ASSERT_EQ(Player::White, move.player);

    if (move.row == 7) {
        ASSERT_NE(7, move.col);
    }
}

// Simple block test - if opponent has 4, bot MUST block
TEST_CASE(TestBotBlockThreat) {
    GomokuBot bot;
    Board board(15);
    bot.setMode(BotMode::Instant);  // Instant should be smart enough for immediate threats usually

    // Black has 4 in a row: (7,5), (7,6), (7,7), (7,8)
    // Threat is at (7,4) and (7,9)
    board.placeStone(7, 5, Player::Black);
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 7, Player::Black);
    board.placeStone(7, 8, Player::Black);

    // White to move
    auto moveOpt = bot.chooseMove(board, Player::White);
    ASSERT_TRUE(moveOpt.has_value());
    Move move = moveOpt.value();

    // Expect blocking at (7,4) or (7,9)
    bool blocked = (move.row == 7 && (move.col == 4 || move.col == 9));
    ASSERT_TRUE(blocked);
}
