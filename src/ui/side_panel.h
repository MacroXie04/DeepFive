#ifndef SIDE_PANEL_H
#define SIDE_PANEL_H

#include <functional>
#include <string>
#include <vector>

#include "../game/game.h"  // For Player enum, etc.
#include "bobcat_ui/all.h"
#include "components.h"

class SidePanel {
   public:
    SidePanel(int x, int y, int w, int h);

    // Layout
    void setupUI();

    // Accessors for state
    int getInitialTime() const;
    void setInitialTime(int seconds);

    // Widget Access (or wrap these)
    // Exposing widgets directly for now to minimize refactoring friction,
    // but ideally should be wrapped.
    bobcat::Button& getBtnNewGame() { return btnNewGame; }
    bobcat::Button& getBtnUndo() { return btnUndo; }
    bobcat::Dropdown& getDdMode() { return ddMode; }
    bobcat::Dropdown& getDdBotSide() { return ddBotSide; }
    bobcat::Dropdown& getDdBotStrength() { return ddBotStrength; }
    bobcat::Checkbox& getChkSelfPlay() { return chkSelfPlay; }
    bool isSelfPlayEnabled() const { return chkSelfPlay.value() == 1; }

    // Update Methods
    void updateTurnIndicator(Player currentPlayer, bool isHistoryEmpty, GameState state,
                             Player winner);
    void updateTimerVisibility(bool showSetup);
    void updateTimerBlocks(double blackTime, double whiteTime, bool isActive, Player activePlayer,
                           bool flash);
    void updateBotSideControl(bool active);
    void updateModeDescription(int strengthIndex);
    void updateStats(const std::string& text);
    void setWinRate(double rate);
    void setProgress(double progress);

   private:
    int panelX, panelY, panelWidth, panelHeight;
    int currentY;
    void advanceY(int height);

    // Widgets
    bobcat::TextBox txtTurn;
    bobcat::Button btnNewGame;
    bobcat::Button btnUndo;

    // Timer Widgets
    bobcat::TextBox txtTimerSetupTitle;
    bobcat::IntInput inputInitialTime;
    bobcat::TextBox txtTimerHint;
    bobcat::TextBox timerBlackBlock;
    bobcat::TextBox timerWhiteBlock;
    std::vector<Fl_Widget*> timerSetupWidgets;
    std::vector<Fl_Widget*> timerDisplayWidgets;

    // Settings
    bobcat::Dropdown ddMode;
    bobcat::Dropdown ddBotSide;
    bobcat::Dropdown ddBotStrength;
    bobcat::Checkbox chkSelfPlay;
    bobcat::TextBox txtModeDesc;

    // Stats
    WinRateBar winRateBar;
    ProgressBar progressBar;
    bobcat::TextBox txtStats;

    // Helper
    std::string formatTimerText(double seconds);
};

#endif
