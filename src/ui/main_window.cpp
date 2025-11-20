#include "main_window.h"
#include "../ai/benchmark.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <FL/Fl.H>

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
      // Initialize widgets with dummy values, will be resized/positioned in setupUI
      txtTurn(0,0,0,0,"Ready"),
      btnNewGame(0,0,0,0,"New Game"),
      btnUndo(0,0,0,0,"Undo"),
      txtTimerSetupTitle(0,0,0,0,"Timer Setup (sec)"),
      inputInitialTime(0,0,0,0,""),
      txtTimerHint(0,0,0,0,"Press New Game after adjusting times."),
      timerBlackBlock(0,0,0,0,"Black 05:00"),
      timerWhiteBlock(0,0,0,0,"White 05:00"),
      ddMode(0,0,0,0,"Game Mode"),
      ddBotSide(0,0,0,0,"Bot Side"),
      ddBotStrength(0,0,0,0,"DeepFive Mode"),
      txtModeDesc(0,0,0,0,"Decides how long to think"),
      winRateBar(0,0,0,0),
      progressBar(0,0,0,0),
      txtStats(0,0,0,0,"")
{
    // Initialize state
    blackConfiguredSeconds = 300;
    whiteConfiguredSeconds = 300;
    blackTimeRemaining = 300.0;
    whiteTimeRemaining = 300.0;
    timersRunning = false;
    flashToggle = false;

    setupUI();
    setupCallbacks();
    
    // Initial Config
    applyTimerConfig();
    updateBotSideControl();
    
    // Initial Analysis
    std::cout << "Running Benchmark..." << std::endl;
    int sps = Benchmark::run(bot);
    std::cout << "Benchmark Result: " << sps << " SPS" << std::endl;
    
    if (sps > 10000) {
        ddBotStrength.value(2); // Thinking
        bot.setMode(BotMode::Thinking);
        txtModeDesc.label("Thinks longer for better moves");
    } else {
        ddBotStrength.value(0); // Auto
        bot.setMode(BotMode::Auto);
        txtModeDesc.label("Decides how long to think");
    }
    
    updateAllUI();
}

void MainWindow::advanceY(int height) {
    currentY += height + 8; // 8 is sidebarSpacing
}

void MainWindow::setupUI() {
    const int sidebarWidth = window.w() - canvas.w();
    const int sidebarPadding = 10;
    panelX = canvas.w() + sidebarPadding;
    panelWidth = sidebarWidth - sidebarPadding * 2;
    currentY = sidebarPadding;
    
    // Re-position widgets
    txtTurn.resize(panelX, currentY, panelWidth, 35);
    txtTurn.box(FL_FLAT_BOX);
    txtTurn.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    txtTurn.color(FL_GREEN);
    txtTurn.labelcolor(FL_BLACK);
    advanceY(35);

    btnNewGame.resize(panelX, currentY, panelWidth, 28);
    advanceY(28);
    
    btnUndo.resize(panelX, currentY, panelWidth, 28);
    advanceY(28);
    
    int timerAreaTop = currentY;
    int timerSetupY = timerAreaTop;
    const int sidebarSpacing = 8;
    
    auto advanceTimerSetupY = [&](int height) {
        timerSetupY += height + sidebarSpacing;
    };
    
    txtTimerSetupTitle.resize(panelX, timerSetupY, panelWidth, 18);
    txtTimerSetupTitle.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    timerSetupWidgets.push_back(&txtTimerSetupTitle);
    advanceTimerSetupY(18);

    inputInitialTime.resize(panelX, timerSetupY, panelWidth, 28);
    inputInitialTime.value(blackConfiguredSeconds);
    inputInitialTime.align(FL_ALIGN_LEFT);
    timerSetupWidgets.push_back(&inputInitialTime);
    advanceTimerSetupY(28);
    
    txtTimerHint.resize(panelX, timerSetupY, panelWidth, 36);
    txtTimerHint.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    txtTimerHint.labelsize(11);
    timerSetupWidgets.push_back(&txtTimerHint);
    advanceTimerSetupY(36);
    
    int timerDisplayY = timerAreaTop;
    auto advanceTimerDisplayY = [&](int height) {
        timerDisplayY += height + sidebarSpacing;
    };
    
    timerBlackBlock.resize(panelX, timerDisplayY, panelWidth, 28);
    timerBlackBlock.box(FL_FLAT_BOX);
    timerBlackBlock.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    timerBlackBlock.labelfont(FL_BOLD);
    timerDisplayWidgets.push_back(&timerBlackBlock);
    advanceTimerDisplayY(28);
    
    timerWhiteBlock.resize(panelX, timerDisplayY, panelWidth, 28);
    timerWhiteBlock.box(FL_FLAT_BOX);
    timerWhiteBlock.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    timerWhiteBlock.labelfont(FL_BOLD);
    timerDisplayWidgets.push_back(&timerWhiteBlock);
    advanceTimerDisplayY(28);
    
    auto effectiveBottom = [&](int cy) {
        return (cy > timerAreaTop) ? (cy - sidebarSpacing) : timerAreaTop;
    };
    int timerAreaBottom = std::max(effectiveBottom(timerSetupY), effectiveBottom(timerDisplayY));
    currentY = timerAreaBottom + sidebarSpacing;

    const int dropdownHeight = 28;
    const int labelHeight = 18;
    
    currentY += labelHeight;
    ddMode.resize(panelX, currentY, panelWidth, dropdownHeight);
    ddMode.add("Human vs Bot");
    ddMode.add("Human vs Human");
    ddMode.add("Bot vs Bot");
    ddMode.value(0); 
    advanceY(dropdownHeight);

    currentY += labelHeight;
    ddBotSide.resize(panelX, currentY, panelWidth, dropdownHeight);
    ddBotSide.add("Bot: White");
    ddBotSide.add("Bot: Black");
    ddBotSide.value(0);
    advanceY(dropdownHeight);
    
    currentY += labelHeight;
    ddBotStrength.resize(panelX, currentY, panelWidth, dropdownHeight);
    ddBotStrength.add("Auto");
    ddBotStrength.add("Instant");
    ddBotStrength.add("Thinking");
    ddBotStrength.add("Extended Thinking");
    ddBotStrength.add("Pro");
    advanceY(dropdownHeight);
    
    const int modeDescHeight = 50;
    txtModeDesc.resize(panelX, currentY, panelWidth, modeDescHeight);
    txtModeDesc.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);
    txtModeDesc.labelsize(11);
    txtModeDesc.labelcolor(FL_GRAY0);
    advanceY(modeDescHeight);
    
    const int winRateBarHeight = 18;
    winRateBar.resize(panelX, currentY, panelWidth, winRateBarHeight);
    advanceY(winRateBarHeight);
    
    const int progressBarHeight = 8;
    progressBar.resize(panelX, currentY, panelWidth, progressBarHeight);
    advanceY(progressBarHeight);

    // Stats
    std::stringstream initStats;
    initStats << "Analysis:\nSPS: 0\nSims: 0";
    
    int remainingHeight = window.h() - currentY - sidebarPadding;
    const int statsHeight = std::max(80, remainingHeight);
    txtStats.resize(panelX, currentY, panelWidth, statsHeight);
    txtStats.label(initStats.str().c_str()); // Careful with string lifetime if not copying
    // Bobcat TextBox copies label? Usually FLTK doesn't copy.
    // But here we set initial value.
    txtStats.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT);
    txtStats.labelcolor(FL_GRAY0);
    txtStats.labelsize(11);
    advanceY(statsHeight);
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
            progressBar.setProgress(0.0);
            Fl::check();
            
            bot.setSearchCallback([this](double winRate, int sims, double elapsedSec, double progress) {
                std::string currentStats = "Reasoning...\n" + formatStats(winRate, sims, elapsedSec);
                txtStats.label(currentStats.c_str());
                txtStats.redraw(); 
                progressBar.setProgress(progress);
                lastStats = "Last Move:\n" + formatStats(winRate, sims, elapsedSec);
            });

            if (game.playBotMove(bot)) {
                canvas.redraw();
                updateAllUI();
                txtStats.label(lastStats.c_str());
                txtStats.redraw();
                progressBar.setProgress(1.0);
                startBackgroundAnalysis();

                bool continueBotBattle = (getSelectedGameMode() == GameMode::BotVsBot && game.getState() == GameState::Playing);
                if (continueBotBattle) {
                    Fl::add_timeout(0.05, [](void* data) {
                        auto fn = static_cast<std::function<void()>*>(data);
                        (*fn)();
                    }, &tryBotPlay);
                }
            }
        }
    };

    btnNewGame.onClick([this](bobcat::Widget* w) {
        applyTimerConfig();
        analysisBot.stopAnalysis();
        game.reset();
        
        GameMode m = getSelectedGameMode();
        game.setMode(m);
        
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
        if (m == GameMode::HumanVsBot) {
            game.setBotSide(botSide);
        }
        
        txtStats.label("New Game Started");
        lastStats = "";
        winRateBar.setWinRate(50.0);
        progressBar.setProgress(0.0);
        
        canvas.redraw();
        updateAllUI();
        
        startBackgroundAnalysis();
        tryBotPlay();
    });

    btnUndo.onClick([this](bobcat::Widget* w) {
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
            txtStats.label("Undo performed");
            progressBar.setProgress(0.0);
            
            startBackgroundAnalysis();
        }
    });

    ddMode.onChange([this](bobcat::Widget* w) {
        analysisBot.stopAnalysis();
        GameMode m = getSelectedGameMode();
        game.setMode(m);
        if (m == GameMode::HumanVsBot) {
            Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
            game.setBotSide(botSide);
        }
        updateBotSideControl();
        updateAllUI();
        startBackgroundAnalysis();
        tryBotPlay();
    });
    
    ddBotStrength.onChange([this](bobcat::Widget* w) {
        int v = ddBotStrength.value();
        std::string desc = "";
        if (v == 0) {
             bot.setMode(BotMode::Auto);
             desc = "Decides how long to think";
        }
        else if (v == 1) {
            bot.setMode(BotMode::Instant);
            desc = "Answers right away";
        }
        else if (v == 2) {
            bot.setMode(BotMode::Thinking);
            desc = "Thinks longer for better moves";
        }
        else if (v == 3) {
            bot.setMode(BotMode::ExtendedThinking);
            desc = "Thinks much longer for stronger moves";
        }
        else {
            bot.setMode(BotMode::Pro);
            desc = "Uses maximum thinking for the best play";
        }
        txtModeDesc.label(desc.c_str());
        txtModeDesc.redraw();
    });

    ddBotSide.onChange([this](bobcat::Widget* w) {
        if (getSelectedGameMode() != GameMode::HumanVsBot) {
            return;
        }
        analysisBot.stopAnalysis();
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
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
        if (w == Player::Black) status += "Black Wins!";
        else if (w == Player::White) status += "White Wins!";
        else status += "Draw!";
    } else {
        Player p = game.getCurrentPlayer();
        status += (p == Player::Black ? "Black's Turn" : "White's Turn");
        if (game.isBotTurn()) {
            status += " (Bot thinking...)";
        }
    }
    window.label(status.c_str());
}

void MainWindow::updateTurnIndicator() {
    if (game.getHistory().empty()) {
        txtTurn.color(FL_GREEN);
        txtTurn.labelcolor(FL_BLACK);
        txtTurn.label("Ready");
    } else if (game.getState() == GameState::Finished) {
         Player w = game.getWinner();
         if (w == Player::Black) {
             txtTurn.color(FL_BLACK);
             txtTurn.labelcolor(FL_WHITE);
             txtTurn.label("Black Wins!");
         } else if (w == Player::White) {
             txtTurn.color(FL_WHITE);
             txtTurn.labelcolor(FL_BLACK);
             txtTurn.label("White Wins!");
         } else {
             txtTurn.color(FL_YELLOW);
             txtTurn.labelcolor(FL_BLACK);
             txtTurn.label("Draw!");
         }
    } else {
        Player p = game.getCurrentPlayer();
        if (p == Player::Black) {
            txtTurn.color(FL_BLACK);
            txtTurn.labelcolor(FL_WHITE);
            txtTurn.label("Black's Turn");
        } else {
            txtTurn.color(FL_WHITE);
            txtTurn.labelcolor(FL_BLACK);
            txtTurn.label("White's Turn");
        }
    }
    txtTurn.redraw();
}

GameMode MainWindow::getSelectedGameMode() {
    int idx = ddMode.value();
    if (idx == 1) return GameMode::HumanVsHuman;
    if (idx == 2) return GameMode::BotVsBot;
    return GameMode::HumanVsBot;
}

void MainWindow::updateBotSideControl() {
    if (getSelectedGameMode() == GameMode::HumanVsBot) {
        ddBotSide.activate();
    } else {
        ddBotSide.deactivate();
    }
}

void MainWindow::updateAllUI() {
    updateTitle();
    updateTurnIndicator();
    refreshTimerPanels();
}

void MainWindow::refreshTimerPanels() {
    bool hasMoves = !game.getHistory().empty();
    bool showSetup = !hasMoves;
    
    for (auto* w : timerSetupWidgets) {
        if (showSetup) w->show();
        else w->hide();
    }
    for (auto* w : timerDisplayWidgets) {
        if (showSetup) w->hide();
        else w->show();
    }
    
    timersRunning = hasMoves && game.getState() == GameState::Playing;
    updateTimerBlocks();
}

void MainWindow::updateTimerBlocks() {
    auto setBlock = [&](bobcat::TextBox& block, const char* prefix, double timeValue, bool isActive, bool isBlackPlayer) {
        std::string text = std::string(prefix) + " " + formatTimerText(timeValue);
        block.copy_label(text.c_str());
        
        const double lowTimeThreshold = 10.0;
        bool isLow = timeValue <= lowTimeThreshold + 0.01;
        Fl_Color color = FL_LIGHT2;
        Fl_Color labelColor = FL_BLACK;
        if (isActive) {
            if (isLow) {
                Fl_Color colorA = isBlackPlayer ? FL_BLACK : FL_WHITE;
                Fl_Color colorB = isBlackPlayer ? fl_rgb_color(40, 40, 40) : fl_rgb_color(220, 220, 220);
                color = flashToggle ? colorA : colorB;
                labelColor = (isBlackPlayer ? FL_WHITE : FL_BLACK);
            } else {
                color = isBlackPlayer ? FL_BLACK : FL_WHITE;
                labelColor = (isBlackPlayer ? FL_WHITE : FL_BLACK);
            }
        } else if (!isActive && isLow) {
            color = FL_YELLOW;
            labelColor = FL_BLACK;
        }
        block.color(color);
        block.labelcolor(labelColor);
        block.redraw();
    };

    bool hasMoves = !game.getHistory().empty();
    bool playing = (game.getState() == GameState::Playing);
    Player active = game.getCurrentPlayer();

    setBlock(timerBlackBlock, "Black", blackTimeRemaining, hasMoves && playing && active == Player::Black, true);
    setBlock(timerWhiteBlock, "White", whiteTimeRemaining, hasMoves && playing && active == Player::White, false);
}

void MainWindow::applyTimerConfig() {
    if (inputInitialTime.empty()) inputInitialTime.value(blackConfiguredSeconds);
    int configured = std::max(1, inputInitialTime.value());
    blackConfiguredSeconds = configured;
    whiteConfiguredSeconds = configured;
    blackTimeRemaining = static_cast<double>(blackConfiguredSeconds);
    whiteTimeRemaining = static_cast<double>(whiteConfiguredSeconds);
    flashToggle = false;
    refreshTimerPanels();
}

void MainWindow::handleTimeoutLoss(Player loser) {
    timersRunning = false;
    analysisBot.stopAnalysis();
    Player winnerPlayer = (loser == Player::Black) ? Player::White : Player::Black;
    game.forceWin(winnerPlayer);
    std::string msg = (loser == Player::Black ? "Black" : "White");
    msg += " ran out of time";
    txtStats.label(msg.c_str());
    txtStats.redraw();
    canvas.redraw();
    updateAllUI();
}

void MainWindow::timerTickLogic() {
    bool hasMoves = !game.getHistory().empty();
    if (!hasMoves) {
        flashToggle = !flashToggle;
        updateTimerBlocks();
        return;
    }

    if (!timersRunning || game.getState() != GameState::Playing) {
        flashToggle = !flashToggle;
        updateTimerBlocks();
        return;
    }

    Player active = game.getCurrentPlayer();
    double& activeTime = (active == Player::Black) ? blackTimeRemaining : whiteTimeRemaining;
    activeTime -= 0.5; // timerIntervalSeconds
    if (activeTime <= 0.0) {
        activeTime = 0.0;
        handleTimeoutLoss(active);
        return;
    }

    flashToggle = !flashToggle;
    updateTimerBlocks();
}

void MainWindow::startBackgroundAnalysis() {
    analysisBot.stopAnalysis();

    if (game.getState() == GameState::Finished) {
        Player w = game.getWinner();
        if (w == Player::Black) winRateBar.setWinRate(100.0);
        else if (w == Player::White) winRateBar.setWinRate(0.0);
        else winRateBar.setWinRate(50.0);
        return;
    }

    if (game.isBotTurn()) {
        return;
    }
    
    Player current = game.getCurrentPlayer();
    
    analysisBot.startAnalysis(game.getBoard(), current, 
        [this, current](double winRate, int sims, double elapsedSec) {
            double blackWinRate = (current == Player::Black) ? winRate : (100.0 - winRate);
            winRateBar.setWinRate(blackWinRate);
            
            if (!game.isBotTurn()) {
                std::string currentStats = "Analysis:\n" + formatStats(winRate, sims, elapsedSec);
                txtStats.label(currentStats.c_str());
                txtStats.redraw();
            }
        });
}

std::string MainWindow::formatStats(double winRate, int sims, double elapsedSec) {
    (void)winRate;
    std::stringstream ss;
    int sps = (elapsedSec > 0.001) ? (int)(sims / elapsedSec) : 0;
    ss << "SPS: " << sps << "\n"
       << "Sims: " << sims;
    return ss.str();
}

std::string MainWindow::formatTimerText(double seconds) {
    if (seconds < 0) seconds = 0;
    int total = static_cast<int>(std::round(seconds));
    int mins = total / 60;
    int secs = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    return std::string(buf);
}
