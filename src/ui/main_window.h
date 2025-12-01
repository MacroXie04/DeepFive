#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <functional>
#include <string>
#include <vector>

#include "../bot/bot.h"
#include "../game/game.h"
#include "bobcat_ui/all.h"
#include "gomoku_canvas.h"
#include "side_panel.h"

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
    double lastWinRate = 50.0;
    int lastSims = 0;
    double lastElapsed = 0.0;
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
