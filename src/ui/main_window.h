#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "../bobcat_ui/all.h"
#include "../game/game.h"
#include "../ai/bot.h"
#include "gomoku_canvas.h"
#include "components.h"
#include <vector>
#include <string>
#include <functional>

struct TimerLoopData {
    std::function<void()> tick;
    double interval{0.5};
};

class MainWindow {
public:
    MainWindow();
    int run();

private:
    // Window & Core
    bobcat::Window window;
    GomokuGame game;
    GomokuBot bot;
    GomokuBot analysisBot;
    GomokuCanvas canvas;

    // Layout Helpers
    int panelX;
    int panelWidth;
    int currentY;
    void advanceY(int height);

    // Widgets
    bobcat::TextBox txtTurn;
    bobcat::Button btnNewGame;
    bobcat::Button btnUndo;
    
    // Timer Widgets
    std::vector<Fl_Widget*> timerSetupWidgets;
    std::vector<Fl_Widget*> timerDisplayWidgets;
    bobcat::TextBox txtTimerSetupTitle;
    bobcat::IntInput inputInitialTime;
    bobcat::TextBox txtTimerHint;
    bobcat::TextBox timerBlackBlock;
    bobcat::TextBox timerWhiteBlock;

    // Settings
    bobcat::Dropdown ddMode;
    bobcat::Dropdown ddBotSide;
    bobcat::Dropdown ddBotStrength;
    bobcat::TextBox txtModeDesc;

    // Stats
    WinRateBar winRateBar;
    ProgressBar progressBar;
    bobcat::TextBox txtStats;

    // State
    int blackConfiguredSeconds;
    int whiteConfiguredSeconds;
    double blackTimeRemaining;
    double whiteTimeRemaining;
    bool timersRunning;
    bool flashToggle;
    std::string lastStats;
    std::function<void()> tryBotPlay;

    // Methods
    void setupUI();
    void setupCallbacks();
    void updateTitle();
    void updateTurnIndicator();
    void updateBotSideControl();
    void updateAllUI();
    void refreshTimerPanels();
    void updateTimerBlocks();
    void applyTimerConfig();
    void handleTimeoutLoss(Player loser);
    void timerTickLogic();
    void startBackgroundAnalysis();
    
    // Helpers
    std::string formatStats(double winRate, int sims, double elapsedSec);
    std::string formatTimerText(double seconds);
    GameMode getSelectedGameMode();
};

#endif
