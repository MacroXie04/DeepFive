#include "main_window.h"

#include <FL/Fl.H>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

// Convert a regular letter to Unicode bold letter (Mathematical Sans-Serif Bold)
static std::string toBoldChar(char c) {
    if (c >= 'A' && c <= 'Z') {
        // Bold uppercase: U+1D5D4 + offset
        char32_t bold = 0x1D5D4 + (c - 'A');
        // Convert to UTF-8
        std::string utf8;
        utf8 += (char)(0xF0 | ((bold >> 18) & 0x07));
        utf8 += (char)(0x80 | ((bold >> 12) & 0x3F));
        utf8 += (char)(0x80 | ((bold >> 6) & 0x3F));
        utf8 += (char)(0x80 | (bold & 0x3F));
        return utf8;
    } else if (c >= 'a' && c <= 'z') {
        // Bold lowercase: U+1D5EE + offset
        char32_t bold = 0x1D5EE + (c - 'a');
        // Convert to UTF-8
        std::string utf8;
        utf8 += (char)(0xF0 | ((bold >> 18) & 0x07));
        utf8 += (char)(0x80 | ((bold >> 12) & 0x3F));
        utf8 += (char)(0x80 | ((bold >> 6) & 0x3F));
        utf8 += (char)(0x80 | (bold & 0x3F));
        return utf8;
    }
    return std::string(1, c);
}

// Helper function to create wave animation with bold characters moving through
static std::string waveAnimateText(const std::string& text, double elapsedSec) {
    std::string result;
    int wavePos = ((int)(elapsedSec * 5)) % ((int)text.length() + 2);  // Wave position

    for (size_t i = 0; i < text.length(); ++i) {
        int dist = std::abs((int)i - wavePos);
        if (dist <= 1 && std::isalpha(text[i])) {
            // Characters near wave position are bold
            result += toBoldChar(text[i]);
        } else {
            result += text[i];
        }
    }
    return result;
}

#include "../bot/benchmark.h"

// Global callback wrapper
void timerLoopCallback(void* userdata) {
    auto data = static_cast<TimerLoopData*>(userdata);
    if (data && data->tick) {
        data->tick();
    }
    if (data) {
        Fl::repeat_timeout(data->interval, timerLoopCallback, userdata);
    }
}

MainWindow::MainWindow()
    : window(850, 600, "DeepFive Gomoku"),
      game(15),
      canvas(0, 0, 600, 600, "", &game),
      sidePanel(610, 10, 230, 580) {
    // Initialize state
    blackConfiguredSeconds = 300;
    whiteConfiguredSeconds = 300;
    blackTimeRemaining = 300.0;
    whiteTimeRemaining = 300.0;
    timersRunning = false;
    flashToggle = false;

    sidePanel.setupUI();
    sidePanel.setInitialTime(blackConfiguredSeconds);
    setupCallbacks();

    // Initial Config
    applyTimerConfig();

    // Initial Analysis
    std::cout << "Running Benchmark..." << std::endl;
    int sps = Benchmark::run(bot);
    std::cout << "Benchmark Result: " << sps << " SPS" << std::endl;

    // Order: Instant(0), Auto(1), Thinking(2), Pro(3)
    if (sps > 10000) {
        sidePanel.getDdBotStrength().value(2);  // Thinking mode
    } else {
        sidePanel.getDdBotStrength().value(1);  // Auto mode (default)
    }

    // updateAllUI();
}

void MainWindow::setupCallbacks() {
    // Timer Loop
    auto* timerLoop = new TimerLoopData();
    timerLoop->interval = 0.5;
    timerLoop->tick = [this]() { this->timerTickLogic(); };
    Fl::add_timeout(timerLoop->interval, timerLoopCallback, timerLoop);

    // Bot Play Logic
    tryBotPlay = [this]() {
        if (game.isBotTurn()) {
            analysisBot.stopAnalysis();
            updateAllUI();
            sidePanel.setProgress(0.0);

            bool isBotVsBot = (getSelectedGameMode() == GameMode::BotVsBot);
            Player thinkingPlayer = game.getCurrentPlayer();
            std::string playerName = (thinkingPlayer == Player::Black) ? "Black" : "White";

            // Build mode info string
            std::string modeInfo = std::string("Mode: ") + bot.getModeName();
            if (bot.isSelfPlayMode()) {
                modeInfo += " + Tournament";
            }

            // Show "Thinking..." immediately before bot starts
            std::string thinkingPrefix =
                isBotVsBot ? (playerName + " thinking:\n") : "Reasoning...\n";
            sidePanel.updateStats(thinkingPrefix + modeInfo + "\nAlgorithm: Searching...");
            Fl::check();

            bot.setSearchCallback(
                [this, playerName, thinkingPrefix, modeInfo](double winRate, int sims,
                                                             double elapsedSec, double progress) {
                    // Wave animation on "Reasoning" or player name
                    std::string animatedPrefix;
                    if (thinkingPrefix.find("Reasoning") != std::string::npos) {
                        animatedPrefix = waveAnimateText("Reasoning", elapsedSec) + "...\n";
                    } else {
                        animatedPrefix =
                            playerName + " " + waveAnimateText("thinking", elapsedSec) + ":\n";
                    }

                    // During MCTS search, show algorithm with wave animation
                    std::string currentStats = animatedPrefix + modeInfo;
                    currentStats += "\nAlgorithm: " + waveAnimateText("MCTS", elapsedSec) + "\n";
                    currentStats += formatStats(winRate, sims, elapsedSec);
                    sidePanel.updateStats(currentStats);
                    sidePanel.setProgress(progress);

                    // Update win rate in real-time during bot thinking
                    double blackWinRate = (playerName == "Black") ? winRate : (100.0 - winRate);
                    sidePanel.setWinRate(blackWinRate);

                    lastWinRate = winRate;
                    lastSims = sims;
                    lastElapsed = elapsedSec;
                });

            // Set up candidate visualization callback for self-play mode
            bot.setCandidateCallback(
                [this, thinkingPlayer](
                    const std::vector<std::tuple<int, int, Player, float>>& candidates) {
                    std::vector<PreviewMove> previews;

                    // Find max score
                    float maxScore = 0.0f;
                    for (const auto& c : candidates) {
                        float score = std::get<3>(c);
                        if (score > maxScore) maxScore = score;
                    }

                    // Only show candidates with score >= 40% of max score
                    const float threshold = maxScore * 0.4f;
                    const int maxDisplay = 8;  // Limit display count

                    for (const auto& c : candidates) {
                        float score = std::get<3>(c);
                        if (score >= threshold && (int)previews.size() < maxDisplay) {
                            PreviewMove pm;
                            pm.row = std::get<0>(c);
                            pm.col = std::get<1>(c);
                            pm.player = thinkingPlayer;
                            pm.score = score;
                            previews.push_back(pm);
                        }
                    }
                    canvas.setPreviewMoves(previews);
                });

            if (game.playBotMove(bot)) {
                canvas.clearPreviewMoves();  // Clear visualization after move
                canvas.redraw();
                updateAllUI();

                // Show detailed stats after move
                std::string algorithmUsed = getAlgorithmStageName(bot.getLastAlgorithmStage());
                std::string modeUsed = std::string(bot.getModeName());
                if (bot.isSelfPlayMode()) {
                    modeUsed += " + Tournament";
                }
                std::string detailedStats = "Last Move:\n";
                detailedStats += "Mode: " + modeUsed + "\n";
                detailedStats += "Algorithm: " + algorithmUsed + "\n";
                detailedStats += formatStats(lastWinRate, lastSims, lastElapsed);
                sidePanel.updateStats(detailedStats);
                sidePanel.setProgress(1.0);
                startBackgroundAnalysis();

                bool continueBotBattle = (isBotVsBot && game.getState() == GameState::Playing);
                if (continueBotBattle) {
                    Fl::add_timeout(
                        0.05,
                        [](void* data) {
                            auto fn = static_cast<std::function<void()>*>(data);
                            (*fn)();
                        },
                        &tryBotPlay);
                }
            }
        }
    };

    sidePanel.getBtnNewGame().onClick([this](bobcat::Widget* w) {
        applyTimerConfig();
        analysisBot.stopAnalysis();
        game.reset();

        GameMode m = getSelectedGameMode();
        game.setMode(m);

        Player botSide = (sidePanel.getDdBotSide().value() == 0) ? Player::White : Player::Black;
        if (m == GameMode::HumanVsBot) {
            game.setBotSide(botSide);
        }

        sidePanel.updateStats("New Game Started");
        lastStats = "";
        sidePanel.setWinRate(50.0);
        sidePanel.setProgress(0.0);

        canvas.redraw();
        updateAllUI();

        startBackgroundAnalysis();
        tryBotPlay();
    });

    sidePanel.getBtnUndo().onClick([this](bobcat::Widget* w) {
        if (game.canUndo()) {
            analysisBot.stopAnalysis();
            game.undoLastMove();

            if (getSelectedGameMode() == GameMode::HumanVsBot) {
                if (game.isBotTurn() && game.canUndo()) {
                    game.undoLastMove();
                }
            }

            canvas.redraw();
            updateAllUI();
            sidePanel.updateStats("Undo performed");
            sidePanel.setProgress(0.0);

            startBackgroundAnalysis();
        }
    });

    sidePanel.getDdMode().onChange([this](bobcat::Widget* w) {
        analysisBot.stopAnalysis();
        GameMode m = getSelectedGameMode();
        game.setMode(m);
        if (m == GameMode::HumanVsBot) {
            Player botSide =
                (sidePanel.getDdBotSide().value() == 0) ? Player::White : Player::Black;
            game.setBotSide(botSide);
        }

        // Update BotSideControl
        bool active = (m == GameMode::HumanVsBot);
        sidePanel.updateBotSideControl(active);

        updateAllUI();
        startBackgroundAnalysis();
        tryBotPlay();
    });

    sidePanel.getDdBotStrength().onChange([this](bobcat::Widget* w) {
        int v = sidePanel.getDdBotStrength().value();
        // Order: Instant(0), Auto(1), Thinking(2), Pro(3)
        if (v == 0)
            bot.setMode(BotMode::Instant);
        else if (v == 1)
            bot.setMode(BotMode::Auto);
        else if (v == 2)
            bot.setMode(BotMode::Thinking);
        else
            bot.setMode(BotMode::Pro);

        sidePanel.updateModeDescription(v);
    });

    sidePanel.getChkSelfPlay().onClick([this](bobcat::Widget* w) {
        bool enabled = sidePanel.isSelfPlayEnabled();
        bot.setSelfPlayMode(enabled);
    });

    sidePanel.getDdBotSide().onChange([this](bobcat::Widget* w) {
        if (getSelectedGameMode() != GameMode::HumanVsBot) {
            return;
        }
        analysisBot.stopAnalysis();
        Player botSide = (sidePanel.getDdBotSide().value() == 0) ? Player::White : Player::Black;
        game.setBotSide(botSide);
        canvas.redraw();
        updateAllUI();
        startBackgroundAnalysis();
        tryBotPlay();
    });

    canvas.onMove([this]() {
        analysisBot.stopAnalysis();
        updateAllUI();
        startBackgroundAnalysis();
        tryBotPlay();
    });
}

int MainWindow::run() {
    window.show();
    Fl::lock();
    startBackgroundAnalysis();
    int exitCode = Fl::run();
    analysisBot.stopAnalysis();
    return exitCode;
}

// Helper Implementations
void MainWindow::updateTitle() {
    std::string status = "DeepFive - ";
    if (game.getState() == GameState::Finished) {
        Player w = game.getWinner();
        if (w == Player::Black)
            status += "Black Wins!";
        else if (w == Player::White)
            status += "White Wins!";
        else
            status += "Draw!";
    } else {
        Player p = game.getCurrentPlayer();
        status += (p == Player::Black ? "Black's Turn" : "White's Turn");
        if (game.isBotTurn()) {
            status += " (Bot thinking...)";
        }
    }
    window.label(status.c_str());
}

GameMode MainWindow::getSelectedGameMode() {
    int idx = sidePanel.getDdMode().value();
    if (idx == 1) return GameMode::HumanVsHuman;
    if (idx == 2) return GameMode::BotVsBot;
    return GameMode::HumanVsBot;
}

void MainWindow::updateAllUI() {
    updateTitle();
    sidePanel.updateTurnIndicator(game.getCurrentPlayer(), game.getHistory().empty(),
                                  game.getState(), game.getWinner());

    bool hasMoves = !game.getHistory().empty();
    bool showSetup = !hasMoves;
    sidePanel.updateTimerVisibility(showSetup);

    timersRunning = hasMoves && game.getState() == GameState::Playing;

    // Update Timer Blocks
    // bool playing = (game.getState() == GameState::Playing);
    Player active = game.getCurrentPlayer();
    sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active,
                                flashToggle);

    // Also update bot side control enabled state
    bool botSideActive = (getSelectedGameMode() == GameMode::HumanVsBot);
    sidePanel.updateBotSideControl(botSideActive);
}

void MainWindow::applyTimerConfig() {
    int val = sidePanel.getInitialTime();
    if (val == 0) {
        sidePanel.setInitialTime(blackConfiguredSeconds);
        val = blackConfiguredSeconds;
    }
    int configured = std::max(1, val);
    blackConfiguredSeconds = configured;
    whiteConfiguredSeconds = configured;
    blackTimeRemaining = static_cast<double>(blackConfiguredSeconds);
    whiteTimeRemaining = static_cast<double>(whiteConfiguredSeconds);
    flashToggle = false;

    // Refresh UI visibility
    bool hasMoves = !game.getHistory().empty();
    sidePanel.updateTimerVisibility(!hasMoves);

    // Update blocks
    Player active = game.getCurrentPlayer();
    sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active,
                                flashToggle);
}

void MainWindow::handleTimeoutLoss(Player loser) {
    timersRunning = false;
    analysisBot.stopAnalysis();
    Player winnerPlayer = (loser == Player::Black) ? Player::White : Player::Black;
    game.forceWin(winnerPlayer);
    std::string msg = (loser == Player::Black ? "Black" : "White");
    msg += " ran out of time";
    sidePanel.updateStats(msg);
    canvas.redraw();
    updateAllUI();
}

void MainWindow::timerTickLogic() {
    bool hasMoves = !game.getHistory().empty();
    if (!hasMoves) {
        flashToggle = !flashToggle;
        Player active = game.getCurrentPlayer();
        sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active,
                                    flashToggle);
        return;
    }

    if (!timersRunning || game.getState() != GameState::Playing) {
        flashToggle = !flashToggle;
        Player active = game.getCurrentPlayer();
        sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active,
                                    flashToggle);
        return;
    }

    Player active = game.getCurrentPlayer();
    double& activeTime = (active == Player::Black) ? blackTimeRemaining : whiteTimeRemaining;
    activeTime -= 0.5;  // timerIntervalSeconds
    if (activeTime <= 0.0) {
        activeTime = 0.0;
        handleTimeoutLoss(active);
        return;
    }

    flashToggle = !flashToggle;
    sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active,
                                flashToggle);
}

void MainWindow::startBackgroundAnalysis() {
    analysisBot.stopAnalysis();

    // Don't analyze when game is finished
    if (game.getState() == GameState::Finished) {
        Player w = game.getWinner();
        if (w == Player::Black)
            sidePanel.setWinRate(100.0);
        else if (w == Player::White)
            sidePanel.setWinRate(0.0);
        else
            sidePanel.setWinRate(50.0);
        return;
    }

    // Don't analyze when no moves have been made (empty board)
    if (game.getHistory().empty()) {
        sidePanel.setWinRate(50.0);  // Equal chances at start
        sidePanel.updateStats("Ready to play");
        return;
    }

    bool isBotVsBot = (getSelectedGameMode() == GameMode::BotVsBot);

    // In Human vs Bot mode, don't analyze during bot's turn (bot computes its own)
    // In Bot vs Bot mode, we still want to show analysis between moves
    if (!isBotVsBot && game.isBotTurn()) {
        return;
    }

    Player current = game.getCurrentPlayer();

    // Get last algorithm used by bot for display
    std::string lastAlgo = getAlgorithmStageName(bot.getLastAlgorithmStage());

    analysisBot.startAnalysis(
        game.getBoard(), current,
        [this, current, isBotVsBot, lastAlgo](double winRate, int sims, double elapsedSec) {
            // Convert to Black's perspective for consistent UI display
            double blackWinRate = (current == Player::Black) ? winRate : (100.0 - winRate);
            sidePanel.setWinRate(blackWinRate);

            // Wave animation on prefix text
            std::string prefix = isBotVsBot ? waveAnimateText("Bot vs Bot", elapsedSec)
                                            : waveAnimateText("Analysis", elapsedSec);
            prefix += "...\n";

            // Update stats text with last algorithm used and animation
            std::string currentStats = prefix;
            currentStats += "Last Algorithm: " + lastAlgo + "\n";
            currentStats += formatStats(winRate, sims, elapsedSec);
            sidePanel.updateStats(currentStats);
        });
}

std::string MainWindow::formatStats(double winRate, int sims, double elapsedSec) {
    (void)winRate;  // Win rate shown in progress bar
    std::stringstream ss;
    int sps = (elapsedSec > 0.001) ? (int)(sims / elapsedSec) : 0;
    ss << "Sims: " << sims << " | SPS: " << sps;
    return ss.str();
}
