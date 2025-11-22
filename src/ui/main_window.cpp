#include "main_window.h"
#include "../bot/benchmark.h"
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
      sidePanel(610, 10, 230, 580)
{
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
    
    if (sps > 10000) {
        sidePanel.getDdBotStrength().value(2); // Triggers onChange to set BotMode and Desc
    } else {
        sidePanel.getDdBotStrength().value(0); // Triggers onChange to set BotMode and Desc
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
            Fl::check();
            
            bot.setSearchCallback([this](double winRate, int sims, double elapsedSec, double progress) {
                std::string currentStats = "Reasoning...\n" + formatStats(winRate, sims, elapsedSec);
                sidePanel.updateStats(currentStats);
                sidePanel.setProgress(progress);
                lastStats = "Last Move:\n" + formatStats(winRate, sims, elapsedSec);
            });

            if (game.playBotMove(bot)) {
                canvas.redraw();
                updateAllUI();
                sidePanel.updateStats(lastStats);
                sidePanel.setProgress(1.0);
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
            Player botSide = (sidePanel.getDdBotSide().value() == 0) ? Player::White : Player::Black;
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
        if (v == 0) bot.setMode(BotMode::Auto);
        else if (v == 1) bot.setMode(BotMode::Instant);
        else if (v == 2) bot.setMode(BotMode::Thinking);
        else if (v == 3) bot.setMode(BotMode::ExtendedThinking);
        else bot.setMode(BotMode::Pro);
        
        sidePanel.updateModeDescription(v);
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

GameMode MainWindow::getSelectedGameMode() {
    int idx = sidePanel.getDdMode().value();
    if (idx == 1) return GameMode::HumanVsHuman;
    if (idx == 2) return GameMode::BotVsBot;
    return GameMode::HumanVsBot;
}

void MainWindow::updateAllUI() {
    updateTitle();
    sidePanel.updateTurnIndicator(game.getCurrentPlayer(), game.getHistory().empty(), game.getState(), game.getWinner());
    
    bool hasMoves = !game.getHistory().empty();
    bool showSetup = !hasMoves;
    sidePanel.updateTimerVisibility(showSetup);
    
    timersRunning = hasMoves && game.getState() == GameState::Playing;
    
    // Update Timer Blocks
    // bool playing = (game.getState() == GameState::Playing);
    Player active = game.getCurrentPlayer();
    sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active, flashToggle);
    
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
    sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active, flashToggle);
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
        sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active, flashToggle);
        return;
    }

    if (!timersRunning || game.getState() != GameState::Playing) {
        flashToggle = !flashToggle;
        Player active = game.getCurrentPlayer();
        sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active, flashToggle);
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
    sidePanel.updateTimerBlocks(blackTimeRemaining, whiteTimeRemaining, timersRunning, active, flashToggle);
}

void MainWindow::startBackgroundAnalysis() {
    analysisBot.stopAnalysis();

    if (game.getState() == GameState::Finished) {
        Player w = game.getWinner();
        if (w == Player::Black) sidePanel.setWinRate(100.0);
        else if (w == Player::White) sidePanel.setWinRate(0.0);
        else sidePanel.setWinRate(50.0);
        return;
    }

    if (game.isBotTurn()) {
        return;
    }
    
    Player current = game.getCurrentPlayer();
    
    analysisBot.startAnalysis(game.getBoard(), current, 
        [this, current](double winRate, int sims, double elapsedSec) {
            double blackWinRate = (current == Player::Black) ? winRate : (100.0 - winRate);
            sidePanel.setWinRate(blackWinRate);
            
            if (!game.isBotTurn()) {
                std::string currentStats = "Analysis:\n" + formatStats(winRate, sims, elapsedSec);
                sidePanel.updateStats(currentStats);
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
