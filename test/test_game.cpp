#include "../src/game/game.h"
#include "test_utils.h"

TEST_CASE(TestGameUndoRedo) {
    GomokuGame game(15);
    game.setMode(GameMode::HumanVsHuman);

    ASSERT_TRUE(game.playHumanMove(7, 7));
    ASSERT_TRUE(game.playHumanMove(7, 8));
    ASSERT_EQ(2, game.getBoard().stoneCount());
    ASSERT_EQ((int)Player::Black, (int)game.getCurrentPlayer());

    game.undoLastMove();
    ASSERT_EQ(1, game.getBoard().stoneCount());
    ASSERT_EQ((int)Player::White, (int)game.getCurrentPlayer());
    ASSERT_TRUE(game.canRedo());

    game.redo();
    ASSERT_EQ(2, game.getBoard().stoneCount());
    ASSERT_EQ((int)Player::Black, (int)game.getCurrentPlayer());
    ASSERT_FALSE(game.canRedo());
}

TEST_CASE(TestGameRedoClearedByNewMove) {
    GomokuGame game(15);
    game.setMode(GameMode::HumanVsHuman);

    ASSERT_TRUE(game.playHumanMove(7, 7));
    ASSERT_TRUE(game.playHumanMove(7, 8));
    game.undoLastMove();

    ASSERT_TRUE(game.canRedo());
    ASSERT_TRUE(game.playHumanMove(7, 9));
    ASSERT_FALSE(game.canRedo());
    ASSERT_EQ(2, game.getBoard().stoneCount());
}

TEST_CASE(TestUndoAfterWinRestoresPlayingState) {
    GomokuGame game(15);
    game.setMode(GameMode::HumanVsHuman);

    ASSERT_TRUE(game.playHumanMove(7, 0));
    ASSERT_TRUE(game.playHumanMove(8, 0));
    ASSERT_TRUE(game.playHumanMove(7, 1));
    ASSERT_TRUE(game.playHumanMove(8, 1));
    ASSERT_TRUE(game.playHumanMove(7, 2));
    ASSERT_TRUE(game.playHumanMove(8, 2));
    ASSERT_TRUE(game.playHumanMove(7, 3));
    ASSERT_TRUE(game.playHumanMove(8, 3));
    ASSERT_TRUE(game.playHumanMove(7, 4));

    ASSERT_EQ((int)GameState::Finished, (int)game.getState());
    ASSERT_EQ((int)Player::Black, (int)game.getWinner());
    ASSERT_FALSE(game.getWinningLine().empty());

    game.undoLastMove();
    ASSERT_EQ((int)GameState::Playing, (int)game.getState());
    ASSERT_EQ((int)Player::NoPlayer, (int)game.getWinner());
    ASSERT_TRUE(game.getWinningLine().empty());
    ASSERT_EQ((int)Player::Black, (int)game.getCurrentPlayer());
}
