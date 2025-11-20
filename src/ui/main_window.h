#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "bobcat_ui/all.h"
#include "../game/game.h"
#include "../ai/bot.h"
#include "gomoku_canvas.h"
#include "side_panel.h"
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
    SidePanel sidePanel;

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
    void setupCallbacks();
    void updateTitle();
    void updateAllUI();
    void applyTimerConfig();
    void handleTimeoutLoss(Player loser);
    void timerTickLogic();
    void startBackgroundAnalysis();
    
    // Helpers
    std::string formatStats(double winRate, int sims, double elapsedSec);
    GameMode getSelectedGameMode();
};

#endif
