#include "../src/bot/bot.h"
#include "../src/core/board.h"
#include "../src/game/game.h"
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
    ASSERT_EQ(7, move.row);
    ASSERT_EQ(7, move.col);
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

// Bot must take immediate win if available
TEST_CASE(TestBotWinImmediately) {
    GomokuBot bot;
    Board board(15);
    bot.setMode(BotMode::Instant);

    // Black has 4 in a row: (7,5), (7,6), (7,7), (7,8)
    // Black to move -> Must play (7,4) or (7,9) to win
    board.placeStone(7, 5, Player::Black);
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 7, Player::Black);
    board.placeStone(7, 8, Player::Black);

    auto moveOpt = bot.chooseMove(board, Player::Black);
    ASSERT_TRUE(moveOpt.has_value());
    Move move = moveOpt.value();

    bool won = (move.row == 7 && (move.col == 4 || move.col == 9));
    ASSERT_TRUE(won);
}

// Bot must block opponent's Live 3 which would become Live 4
TEST_CASE(TestBotPreventOpenFour) {
    GomokuBot bot;
    Board board(15);
    bot.setMode(BotMode::Instant);

    // Black has Live 3: (7,6), (7,7), (7,8)
    // Spaces at (7,5) and (7,9) are open.
    // If Black plays (7,5), it becomes Live 4 (X X X X).
    // Bot (White) must block (7,5) or (7,9).
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 7, Player::Black);
    board.placeStone(7, 8, Player::Black);

    auto moveOpt = bot.chooseMove(board, Player::White);
    ASSERT_TRUE(moveOpt.has_value());
    Move move = moveOpt.value();

    bool blocked = (move.row == 7 && (move.col == 5 || move.col == 9));
    ASSERT_TRUE(blocked);
}

TEST_CASE(TestBotAutoModeReturnsLegalMove) {
    GomokuBot bot;
    Board board(15);
    bot.setMode(BotMode::Auto);

    board.placeStone(7, 7, Player::Black);
    board.placeStone(7, 8, Player::White);
    ASSERT_EQ(2, board.stoneCount());

    auto moveOpt = bot.chooseMove(board, Player::Black);
    ASSERT_TRUE(moveOpt.has_value());
    Move move = moveOpt.value();
    ASSERT_TRUE(board.isInside(move.row, move.col));
    ASSERT_TRUE(board.isEmpty(move.row, move.col));
    ASSERT_EQ(Player::Black, move.player);
}

// Simulate a full game between two bots to ensure stability and termination
TEST_CASE(TestBotVsBotGame) {
    GomokuGame game(15);
    game.setMode(GameMode::BotVsBot);

    GomokuBot botBlack;
    botBlack.setMode(BotMode::Instant);

    GomokuBot botWhite;
    botWhite.setMode(BotMode::Instant);

    int moves = 0;
    while (game.getState() == GameState::Playing && moves < 225) {
        bool moveMade = false;
        if (game.getCurrentPlayer() == Player::Black) {
            moveMade = game.playBotMove(botBlack);
        } else {
            moveMade = game.playBotMove(botWhite);
        }
        ASSERT_TRUE(moveMade);
        moves++;
    }

    ASSERT_EQ((int)GameState::Finished, (int)game.getState());
    std::cout << "Bot vs Bot game finished in " << moves << " moves. Winner: " << game.getWinner()
              << std::endl;
}
