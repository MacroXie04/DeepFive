#include "bobcat_ui/all.h"
#include "game/game.h"
#include "ai/bot.h"
#include "ui/gomoku_canvas.h"
#include "ai/benchmark.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <FL/Enumerations.H>
#include <FL/fl_draw.H>

struct TimerLoopData {
    std::function<void()> tick;
    double interval{0.5};
};

void timerLoopCallback(void* userdata) {
    auto data = static_cast<TimerLoopData*>(userdata);
    if (data && data->tick) {
        data->tick();
    }
    if (data) {
        Fl::repeat_timeout(data->interval, timerLoopCallback, userdata);
    }
}

class WinRateBar : public Fl_Box {
    double winRate; // Black's win rate (0-100)
public:
    WinRateBar(int x, int y, int w, int h) : Fl_Box(x, y, w, h, "") {
        winRate = 50.0;
        box(FL_FLAT_BOX);
    }
    
    void setWinRate(double rate) {
        winRate = rate;
        if (winRate < 0) winRate = 0;
        if (winRate > 100) winRate = 100;
        redraw();
    }
    
    void draw() override {
        // Background (White)
        fl_rectf(x(), y(), w(), h(), FL_WHITE);
        
        // Black portion (Left side)
        int blackW = (int)((winRate / 100.0) * w());
        fl_rectf(x(), y(), blackW, h(), FL_BLACK);
        
        // Border
        fl_color(FL_GRAY0);
        fl_rect(x(), y(), w(), h());
        
        // Text
        fl_font(FL_HELVETICA, 12);
        
        // Draw Black % on the left (white text) if enough space
        if (blackW > 30) {
            fl_color(FL_WHITE);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f%%", winRate);
            fl_draw(buf, x() + 5, y(), blackW - 5, h(), FL_ALIGN_LEFT);
        }
        
        // Draw White % on the right (black text) if enough space
        if (w() - blackW > 30) {
            fl_color(FL_BLACK);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f%%", 100.0 - winRate);
            fl_draw(buf, x() + blackW + 5, y(), w() - blackW - 5, h(), FL_ALIGN_RIGHT);
        }
    }
};

class ProgressBar : public Fl_Box {
    double progress; // 0.0 to 1.0
public:
    ProgressBar(int x, int y, int w, int h) : Fl_Box(x, y, w, h, "") {
        progress = 0.0;
        box(FL_FLAT_BOX);
    }

    void setProgress(double p) {
        if (p < 0) p = 0;
        if (p > 1.0) p = 1.0;
        progress = p;
        redraw();
    }

    void draw() override {
        // Background
        fl_rectf(x(), y(), w(), h(), FL_WHITE);
        
        // Fill
        int fillW = (int)(progress * w());
        fl_color(FL_GREEN);
        fl_rectf(x(), y(), fillW, h());
        
        // Border
        fl_color(FL_GRAY0);
        fl_rect(x(), y(), w(), h());
    }
};

void updateTitle(bobcat::Window& window, const GomokuGame& game) {
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
    window.label(status);
}

void updateTurnIndicator(bobcat::TextBox& box, const GomokuGame& game) {
    if (game.getHistory().empty()) {
        box.color(FL_GREEN);
        box.labelcolor(FL_BLACK);
        box.label("Ready");
    } else if (game.getState() == GameState::Finished) {
         Player w = game.getWinner();
         if (w == Player::Black) {
             box.color(FL_BLACK);
             box.labelcolor(FL_WHITE);
             box.label("Black Wins!");
         } else if (w == Player::White) {
             box.color(FL_WHITE);
             box.labelcolor(FL_BLACK);
             box.label("White Wins!");
         } else {
             box.color(FL_YELLOW);
             box.labelcolor(FL_BLACK);
             box.label("Draw!");
         }
    } else {
        Player p = game.getCurrentPlayer();
        if (p == Player::Black) {
            box.color(FL_BLACK);
            box.labelcolor(FL_WHITE);
            box.label("Black's Turn");
        } else {
            box.color(FL_WHITE);
            box.labelcolor(FL_BLACK);
            box.label("White's Turn");
        }
    }
    box.redraw();
}

int main(int argc, char **argv) {
    bobcat::Window window(850, 600, "DeepFive Gomoku");

    GomokuGame game(15);
    GomokuBot bot;
    GomokuBot analysisBot; // Dedicated for UI stats
    
    int defaultTimerSeconds = 300;
    int blackConfiguredSeconds = defaultTimerSeconds;
    int whiteConfiguredSeconds = defaultTimerSeconds;
    double blackTimeRemaining = static_cast<double>(blackConfiguredSeconds);
    double whiteTimeRemaining = static_cast<double>(whiteConfiguredSeconds);
    const double lowTimeThreshold = 10.0;
    const double timerIntervalSeconds = 0.5;
    bool timersRunning = false;
    bool flashToggle = false;
    
    // Run Benchmark at start
    std::cout << "Running Benchmark..." << std::endl;
    int sps = Benchmark::run(bot);
    std::cout << "Benchmark Result: " << sps << " SPS" << std::endl;

    GomokuCanvas canvas(0, 0, 600, 600, "", &game);
    
    const int sidebarWidth = window.w() - canvas.w();
    const int sidebarPadding = 10;
    int panelX = canvas.w() + sidebarPadding; // 10px inset from canvas edge
    const int panelWidth = sidebarWidth - sidebarPadding * 2;
    int y = sidebarPadding;
    const int sidebarSpacing = 8; // Reduced spacing for better fit
    auto advanceY = [&](int height) {
        y += height + sidebarSpacing;
    };
    
    std::vector<Fl_Widget*> timerSetupWidgets;
    std::vector<Fl_Widget*> timerDisplayWidgets;
    
    // Turn Indicator
    bobcat::TextBox txtTurn(panelX, y, panelWidth, 35, "Ready");
    txtTurn.box(FL_FLAT_BOX);
    txtTurn.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    txtTurn.color(FL_GREEN);
    txtTurn.labelcolor(FL_BLACK);
    advanceY(35);

    bobcat::Button btnNewGame(panelX, y, panelWidth, 28, "New Game");
    advanceY(28);
    
    bobcat::Button btnUndo(panelX, y, panelWidth, 28, "Undo");
    advanceY(28);
    
    int timerAreaTop = y;
    int timerSetupY = timerAreaTop;
    auto advanceTimerSetupY = [&](int height) {
        timerSetupY += height + sidebarSpacing;
    };
    
    bobcat::TextBox txtTimerSetupTitle(panelX, timerSetupY, panelWidth, 18, "Timer Setup (sec)");
    txtTimerSetupTitle.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    timerSetupWidgets.push_back(&txtTimerSetupTitle);
    advanceTimerSetupY(18);

    bobcat::IntInput inputInitialTime(panelX, timerSetupY, panelWidth, 28, "");
    inputInitialTime.value(blackConfiguredSeconds);
    inputInitialTime.align(FL_ALIGN_LEFT);
    timerSetupWidgets.push_back(&inputInitialTime);
    advanceTimerSetupY(28);
    
    bobcat::TextBox txtTimerHint(panelX, timerSetupY, panelWidth, 36, "Press New Game after adjusting times.");
    txtTimerHint.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    txtTimerHint.labelsize(11);
    timerSetupWidgets.push_back(&txtTimerHint);
    advanceTimerSetupY(36);
    
    int timerDisplayY = timerAreaTop;
    auto advanceTimerDisplayY = [&](int height) {
        timerDisplayY += height + sidebarSpacing;
    };
    bobcat::TextBox timerBlackBlock(panelX, timerDisplayY, panelWidth, 28, "Black 05:00");
    timerBlackBlock.box(FL_FLAT_BOX);
    timerBlackBlock.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    timerBlackBlock.labelfont(FL_BOLD);
    timerDisplayWidgets.push_back(&timerBlackBlock);
    advanceTimerDisplayY(28);
    
    bobcat::TextBox timerWhiteBlock(panelX, timerDisplayY, panelWidth, 28, "White 05:00");
    timerWhiteBlock.box(FL_FLAT_BOX);
    timerWhiteBlock.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    timerWhiteBlock.labelfont(FL_BOLD);
    timerDisplayWidgets.push_back(&timerWhiteBlock);
    advanceTimerDisplayY(28);
    
    auto effectiveBottom = [&](int currentY) {
        return (currentY > timerAreaTop) ? (currentY - sidebarSpacing) : timerAreaTop;
    };
    int timerAreaBottom = std::max(effectiveBottom(timerSetupY), effectiveBottom(timerDisplayY));
    y = timerAreaBottom + sidebarSpacing;

    const int dropdownHeight = 28;
    const int labelHeight = 18; // Space reserved for dropdown label above the control
    
    // First dropdown: Game Mode
    y += labelHeight; // Reserve space for label
    bobcat::Dropdown ddMode(panelX, y, panelWidth, dropdownHeight, "Game Mode");
    ddMode.add("Human vs Bot");
    ddMode.add("Human vs Human");
    ddMode.add("Bot vs Bot");
    ddMode.value(0); 
    advanceY(dropdownHeight);

    // Second dropdown: Bot Side
    y += labelHeight; // Reserve space for label
    bobcat::Dropdown ddBotSide(panelX, y, panelWidth, dropdownHeight, "Bot Side");
    ddBotSide.add("Bot: White");
    ddBotSide.add("Bot: Black");
    ddBotSide.value(0);
    advanceY(dropdownHeight);
    
    // Third dropdown: DeepFive Mode
    y += labelHeight; // Reserve space for label
    bobcat::Dropdown ddBotStrength(panelX, y, panelWidth, dropdownHeight, "DeepFive Mode");
    ddBotStrength.add("Auto");
    ddBotStrength.add("Instant");
    ddBotStrength.add("Thinking");
    ddBotStrength.add("Extended Thinking");
    ddBotStrength.add("Pro");
    advanceY(dropdownHeight);

    auto getSelectedGameMode = [&]() -> GameMode {
        int idx = ddMode.value();
        if (idx == 1) return GameMode::HumanVsHuman;
        if (idx == 2) return GameMode::BotVsBot;
        return GameMode::HumanVsBot;
    };

    auto updateBotSideControl = [&]() {
        if (getSelectedGameMode() == GameMode::HumanVsBot) {
            ddBotSide.activate();
        } else {
            ddBotSide.deactivate();
        }
    };

    updateBotSideControl();

    // Description Text Box
    const int modeDescHeight = 50;
    bobcat::TextBox txtModeDesc(panelX, y, panelWidth, modeDescHeight, "Decides how long to think");
    txtModeDesc.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);
    txtModeDesc.labelsize(11);
    txtModeDesc.labelcolor(FL_GRAY0);
    advanceY(modeDescHeight);
    
    if (sps > 10000) {
        ddBotStrength.value(2); // Thinking
        bot.setMode(BotMode::Thinking);
        txtModeDesc.label("Thinks longer for better moves");
    } else {
        ddBotStrength.value(0); // Auto
        bot.setMode(BotMode::Auto);
        txtModeDesc.label("Decides how long to think");
    }
    
    // Win Rate Bar
    const int winRateBarHeight = 18;
    WinRateBar winRateBar(panelX, y, panelWidth, winRateBarHeight);
    advanceY(winRateBarHeight);
    
    // Progress Bar (Hidden when not thinking)
    const int progressBarHeight = 8;
    ProgressBar progressBar(panelX, y, panelWidth, progressBarHeight);
    advanceY(progressBarHeight);

    // Status Text Box - Calculate remaining space
    std::stringstream initStats;
    initStats << "Analysis:\n"
              << "SPS: 0\n"
              << "Sims: 0";
    
    // Calculate remaining height for stats box
    int remainingHeight = window.h() - y - sidebarPadding;
    const int statsHeight = std::max(80, remainingHeight);
    bobcat::TextBox txtStats(panelX, y, panelWidth, statsHeight, initStats.str());
    txtStats.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT);
    txtStats.labelcolor(FL_GRAY0);
    txtStats.labelsize(11);
    advanceY(statsHeight);

    auto formatStats = [&](double winRate, int sims, double elapsedSec) {
        (void)winRate;
        std::stringstream ss;
        int sps = (elapsedSec > 0.001) ? (int)(sims / elapsedSec) : 0;
        ss << "SPS: " << sps << "\n"
           << "Sims: " << sims;
        return ss.str();
    };

    std::string lastStats = "Analysis:\n" + formatStats(0, 0, 0);

    auto formatTimerText = [](double seconds) {
        if (seconds < 0) seconds = 0;
        int total = static_cast<int>(std::round(seconds));
        int mins = total / 60;
        int secs = total % 60;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        return std::string(buf);
    };

    auto showSetupPanel = [&](bool showSetup) {
        for (auto* w : timerSetupWidgets) {
            if (showSetup) w->show();
            else w->hide();
        }
        for (auto* w : timerDisplayWidgets) {
            if (showSetup) w->hide();
            else w->show();
        }
    };

    auto updateTimerBlocks = [&]() {
        auto setBlock = [&](bobcat::TextBox& block, const char* prefix, double timeValue, bool isActive, bool isBlackPlayer) {
            std::string text = std::string(prefix) + " " + formatTimerText(timeValue);
            block.copy_label(text.c_str());
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
    };

    auto refreshTimerPanels = [&]() {
        bool hasMoves = !game.getHistory().empty();
        showSetupPanel(!hasMoves);
        timersRunning = hasMoves && game.getState() == GameState::Playing;
        updateTimerBlocks();
    };

    auto applyTimerConfig = [&]() {
        if (inputInitialTime.empty()) inputInitialTime.value(blackConfiguredSeconds);
        int configured = std::max(1, inputInitialTime.value());
        blackConfiguredSeconds = configured;
        whiteConfiguredSeconds = configured;
        blackTimeRemaining = static_cast<double>(blackConfiguredSeconds);
        whiteTimeRemaining = static_cast<double>(whiteConfiguredSeconds);
        flashToggle = false;
        refreshTimerPanels();
    };

    applyTimerConfig();

    auto updateAllUI = [&]() {
        updateTitle(window, game);
        updateTurnIndicator(txtTurn, game);
        refreshTimerPanels();
    };

    auto handleTimeoutLoss = [&](Player loser) {
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
    };

    auto timerTickLogic = [&]() {
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
        activeTime -= timerIntervalSeconds;
        if (activeTime <= 0.0) {
            activeTime = 0.0;
            handleTimeoutLoss(active);
            return;
        }

        flashToggle = !flashToggle;
        updateTimerBlocks();
    };

    auto* timerLoop = new TimerLoopData();
    timerLoop->interval = timerIntervalSeconds;
    timerLoop->tick = timerTickLogic;
    Fl::add_timeout(timerLoop->interval, timerLoopCallback, timerLoop);

    // Centralized Analysis Starter
    auto startBackgroundAnalysis = [&]() {
        if (game.getState() == GameState::Finished) {
            analysisBot.stopAnalysis();
            // Show 100% or 0%?
            Player w = game.getWinner();
            if (w == Player::Black) winRateBar.setWinRate(100.0);
            else if (w == Player::White) winRateBar.setWinRate(0.0);
            else winRateBar.setWinRate(50.0);
            return;
        }
        
        // If Bot is playing, we might defer or run in parallel.
        // User wants real-time high precision stats.
        // We'll run in parallel.
        
        Player current = game.getCurrentPlayer();
        
        analysisBot.startAnalysis(game.getBoard(), current, 
            [&, current](double winRate, int sims, double elapsedSec) {
                // winRate is for 'current' player.
                // We want Black's win rate.
                double blackWinRate = (current == Player::Black) ? winRate : (100.0 - winRate);
                
                winRateBar.setWinRate(blackWinRate);
                
                // Only update text stats if NOT bot turn (otherwise bot overwrites it)
                if (!game.isBotTurn()) {
                    std::string currentStats = "Analysis:\n" + formatStats(winRate, sims, elapsedSec);
                    txtStats.label(currentStats);
                    txtStats.redraw();
                }
            });
    };

    std::function<void()> tryBotPlay;
    tryBotPlay = [&]() {
        if (game.isBotTurn()) {
            updateAllUI();
            
            // Show progress bar
            progressBar.setProgress(0.0);
            // progressBar.show();
            
            Fl::check();
            
            bot.setSearchCallback([&](double winRate, int sims, double elapsedSec, double progress) {
                std::string currentStats = "Reasoning...\n" + formatStats(winRate, sims, elapsedSec);
                txtStats.label(currentStats);
                txtStats.redraw(); 
                
                progressBar.setProgress(progress);
                
                lastStats = "Last Move:\n" + formatStats(winRate, sims, elapsedSec);
            });

            if (game.playBotMove(bot)) {
                canvas.redraw();
                updateAllUI();
                txtStats.label(lastStats);
                txtStats.redraw();
                
                // Hide progress bar
                progressBar.setProgress(1.0);
                
                // Bot moved, start analysis for Human's turn
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

    btnNewGame.onClick([&](bobcat::Widget* w) {
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

    btnUndo.onClick([&](bobcat::Widget* w) {
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

    ddMode.onChange([&](bobcat::Widget* w) {
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
    
    ddBotStrength.onChange([&](bobcat::Widget* w) {
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
        txtModeDesc.label(desc);
        txtModeDesc.redraw();
    });

    ddBotSide.onChange([&](bobcat::Widget* w) {
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

    canvas.onMove([&]() {
        analysisBot.stopAnalysis(); // Stop old analysis
        updateAllUI();
        
        startBackgroundAnalysis(); // Start new one
        tryBotPlay();
    });
    
    updateAllUI();
    startBackgroundAnalysis(); // Initial analysis

    window.show();
    Fl::lock(); // Enable thread support
    int exitCode = Fl::run();
    analysisBot.stopAnalysis();
    return exitCode;
}
