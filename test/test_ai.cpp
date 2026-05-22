#include "../src/bot/heuristics.h"
#include "../src/bot/mcts.h"
#include "../src/bot/patterns.h"
#include "../src/search/ForcedDFS.h"
#include "test_utils.h"

TEST_CASE(TestPatternDetectsJumpFour) {
    Board board(15);
    board.placeStone(7, 5, Player::Black);
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 8, Player::Black);

    auto threat = Patterns::analyzeMoveThreat(board, 7, 9, Player::Black);
    ASSERT_TRUE(threat.counts.jumpFours > 0 || threat.counts.totalFours() > 0);
    ASSERT_TRUE(Patterns::isForcingMove(threat));
}

TEST_CASE(TestPatternDetectsRushFour) {
    Board board(15);
    board.placeStone(7, 4, Player::White);
    board.placeStone(7, 5, Player::Black);
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 7, Player::Black);

    auto threat = Patterns::analyzeMoveThreat(board, 7, 8, Player::Black);
    ASSERT_TRUE(threat.counts.rushFours > 0 || threat.counts.totalFours() > 0);
    ASSERT_TRUE(Patterns::isForcingMove(threat));
}

TEST_CASE(TestPatternDetectsLiveAndJumpThrees) {
    Board liveBoard(15);
    liveBoard.placeStone(7, 6, Player::Black);
    liveBoard.placeStone(7, 7, Player::Black);
    auto liveThreat = Patterns::analyzeMoveThreat(liveBoard, 7, 8, Player::Black);
    ASSERT_TRUE(liveThreat.counts.liveThrees > 0);

    Board jumpBoard(15);
    jumpBoard.placeStone(7, 6, Player::Black);
    jumpBoard.placeStone(7, 8, Player::Black);
    auto jumpThreat = Patterns::analyzeMoveThreat(jumpBoard, 7, 9, Player::Black);
    ASSERT_TRUE(jumpThreat.counts.jumpLiveThrees > 0 || jumpThreat.counts.liveThrees > 0);
}

TEST_CASE(TestPatternDetectsDoubleThreeAndEdgeBlock) {
    Board doubleThree(15);
    doubleThree.placeStone(7, 6, Player::Black);
    doubleThree.placeStone(7, 8, Player::Black);
    doubleThree.placeStone(6, 7, Player::Black);
    doubleThree.placeStone(8, 7, Player::Black);
    auto threat = Patterns::analyzeMoveThreat(doubleThree, 7, 7, Player::Black);
    ASSERT_TRUE(threat.createsDoubleThree);

    Board edge(15);
    edge.placeStone(0, 0, Player::Black);
    edge.placeStone(0, 1, Player::Black);
    auto edgeThreat = Patterns::analyzeMoveThreat(edge, 0, 2, Player::Black);
    ASSERT_EQ(0, edgeThreat.counts.liveThrees);
}

TEST_CASE(TestCandidateOrderingWinBeforeBlockAndBlockBeforeQuiet) {
    Board winning(15);
    winning.placeStone(7, 5, Player::Black);
    winning.placeStone(7, 6, Player::Black);
    winning.placeStone(7, 7, Player::Black);
    winning.placeStone(7, 8, Player::Black);
    winning.placeStone(6, 5, Player::White);
    winning.placeStone(6, 6, Player::White);
    winning.placeStone(6, 7, Player::White);
    auto winCandidates = Heuristics::getScoredCandidateMoves(winning, Player::Black, {1});
    ASSERT_TRUE(!winCandidates.empty());
    ASSERT_TRUE(winCandidates[0].move.row == 7);
    ASSERT_TRUE(winCandidates[0].move.col == 4 || winCandidates[0].move.col == 9);

    Board blocking(15);
    blocking.placeStone(7, 5, Player::White);
    blocking.placeStone(7, 6, Player::White);
    blocking.placeStone(7, 7, Player::White);
    blocking.placeStone(7, 8, Player::White);
    blocking.placeStone(6, 6, Player::Black);
    auto blockCandidates = Heuristics::getScoredCandidateMoves(blocking, Player::Black, {1});
    ASSERT_TRUE(!blockCandidates.empty());
    ASSERT_TRUE(blockCandidates[0].move.row == 7);
    ASSERT_TRUE(blockCandidates[0].move.col == 4 || blockCandidates[0].move.col == 9);
}

TEST_CASE(TestBeamPruningPreservesForcedMove) {
    Board board(15);
    board.placeStone(7, 5, Player::White);
    board.placeStone(7, 6, Player::White);
    board.placeStone(7, 7, Player::White);
    board.placeStone(7, 8, Player::White);
    board.placeStone(4, 4, Player::Black);
    board.placeStone(5, 5, Player::Black);
    board.placeStone(6, 6, Player::Black);

    Heuristics::CandidateOptions options;
    options.maxMoves = 1;
    auto candidates = Heuristics::getScoredCandidateMoves(board, Player::Black, options);
    bool hasBlock = false;
    for (const auto& candidate : candidates) {
        if (candidate.move.row == 7 && (candidate.move.col == 4 || candidate.move.col == 9)) {
            hasBlock = true;
        }
    }
    ASSERT_TRUE(hasBlock);
}

TEST_CASE(TestVCFUsesJumpFourThreats) {
    Board board(15);
    board.placeStone(7, 5, Player::Black);
    board.placeStone(7, 6, Player::Black);
    board.placeStone(7, 8, Player::Black);

    auto moves = Heuristics::getScoredCandidateMoves(board, Player::Black, {4});
    bool hasJumpFour = false;
    for (const auto& candidate : moves) {
        auto threat = Patterns::analyzeMoveThreat(board, candidate.move.row, candidate.move.col,
                                                  Player::Black);
        if (threat.counts.totalFours() > 0) {
            hasJumpFour = true;
        }
    }
    ASSERT_TRUE(hasJumpFour);
}

TEST_CASE(TestMCTSDeterministicSeedAndStaticEvaluation) {
    Board board(15);
    board.placeStone(7, 7, Player::Black);
    board.placeStone(7, 8, Player::White);
    board.placeStone(8, 7, Player::Black);

    MCTSSolver::Options options;
    options.seed = 1234;
    options.rootBeamSize = 12;
    options.childBeamSize = 8;
    MCTSSolver first(board, Player::White, options);
    MCTSSolver second(board, Player::White, options);

    first.runIterations(40);
    second.runIterations(40);
    auto firstMove = first.getBestMove().first;
    auto secondMove = second.getBestMove().first;

    ASSERT_TRUE(firstMove.has_value());
    ASSERT_TRUE(secondMove.has_value());
    ASSERT_EQ(firstMove->row, secondMove->row);
    ASSERT_EQ(firstMove->col, secondMove->col);

    Board advantage(15);
    advantage.placeStone(7, 5, Player::Black);
    advantage.placeStone(7, 6, Player::Black);
    advantage.placeStone(7, 7, Player::Black);
    auto eval = Evaluation::evaluateBoard(advantage, Player::Black);
    ASSERT_TRUE(eval.winProbability > 0.5f);
}
