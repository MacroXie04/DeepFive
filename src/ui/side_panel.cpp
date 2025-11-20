#include "side_panel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <iostream>

SidePanel::SidePanel(int x, int y, int w, int h)
    : panelX(x), panelY(y), panelWidth(w), panelHeight(h),
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
    // Constructor initializes widgets with dummy rects
}

void SidePanel::advanceY(int height) {
    currentY += height + 8; // 8 is sidebarSpacing
}

void SidePanel::setupUI() {
    const int sidebarPadding = 0; // Already padded by x/y passed in? 
    // MainWindow logic was: panelX = canvas.w() + 10.
    // Here we assume panelX, panelY are the start coordinates.
    
    currentY = panelY;
    
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
    // Initial value should be set by caller or default
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
    // std::stringstream initStats;
    // initStats << "Analysis:\nSPS: 0\nSims: 0";
    // We leave initial text to be set by updateStats or constructor
    
    int remainingHeight = panelHeight - (currentY - panelY); 
    // Note: panelHeight passed in was likely remaining window height or similar, 
    // but here we just use what we have.
    // In MainWindow it calculated against window.h().
    // Let's assume panelHeight is effectively the max Y we can go to.
    
    const int statsHeight = std::max(80, remainingHeight);
    txtStats.resize(panelX, currentY, panelWidth, statsHeight);
    txtStats.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT);
    txtStats.labelcolor(FL_GRAY0);
    txtStats.labelsize(11);
    advanceY(statsHeight);
}

int SidePanel::getInitialTime() const {
    return inputInitialTime.value();
}

void SidePanel::setInitialTime(int seconds) {
    inputInitialTime.value(seconds);
}

void SidePanel::updateTurnIndicator(Player currentPlayer, bool isHistoryEmpty, GameState state, Player winner) {
    if (isHistoryEmpty) {
        txtTurn.color(FL_GREEN);
        txtTurn.labelcolor(FL_BLACK);
        txtTurn.label("Ready");
    } else if (state == GameState::Finished) {
         if (winner == Player::Black) {
             txtTurn.color(FL_BLACK);
             txtTurn.labelcolor(FL_WHITE);
             txtTurn.label("Black Wins!");
         } else if (winner == Player::White) {
             txtTurn.color(FL_WHITE);
             txtTurn.labelcolor(FL_BLACK);
             txtTurn.label("White Wins!");
         } else {
             txtTurn.color(FL_YELLOW);
             txtTurn.labelcolor(FL_BLACK);
             txtTurn.label("Draw!");
         }
    } else {
        if (currentPlayer == Player::Black) {
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

void SidePanel::updateTimerVisibility(bool showSetup) {
    for (auto* w : timerSetupWidgets) {
        if (showSetup) w->show();
        else w->hide();
    }
    for (auto* w : timerDisplayWidgets) {
        if (showSetup) w->hide();
        else w->show();
    }
}

std::string SidePanel::formatTimerText(double seconds) {
    if (seconds < 0) seconds = 0;
    int total = static_cast<int>(std::round(seconds));
    int mins = total / 60;
    int secs = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    return std::string(buf);
}

void SidePanel::updateTimerBlocks(double blackTime, double whiteTime, bool isActive, Player activePlayer, bool flash) {
    auto setBlock = [&](bobcat::TextBox& block, const char* prefix, double timeValue, bool isThisPlayerActive, bool isBlackPlayer) {
        std::string text = std::string(prefix) + " " + formatTimerText(timeValue);
        block.copy_label(text.c_str());
        
        const double lowTimeThreshold = 10.0;
        bool isLow = timeValue <= lowTimeThreshold + 0.01;
        Fl_Color color = FL_LIGHT2;
        Fl_Color labelColor = FL_BLACK;
        
        if (isActive && isThisPlayerActive) {
            if (isLow) {
                Fl_Color colorA = isBlackPlayer ? FL_BLACK : FL_WHITE;
                Fl_Color colorB = isBlackPlayer ? fl_rgb_color(40, 40, 40) : fl_rgb_color(220, 220, 220);
                color = flash ? colorA : colorB;
                labelColor = (isBlackPlayer ? FL_WHITE : FL_BLACK);
            } else {
                color = isBlackPlayer ? FL_BLACK : FL_WHITE;
                labelColor = (isBlackPlayer ? FL_WHITE : FL_BLACK);
            }
        } else if (!isActive && isLow) { 
             // Wait, logic in original was: if (!isActive && isLow).
             // Actually original was: if (isActive) {...} else if (!isActive && isLow) {...}
             // Here 'isActive' param means "Timers are Running".
             // 'isThisPlayerActive' means "It is this player's turn".
             // The original code:
             // if (isActive) { // isActive means 'playing && active == Player::This'
             //    ...
             // } else if (!isActive && isLow) { ... }
             // 
             // My param `isActive` in updateTimerBlocks is `timersRunning`.
             // `isThisPlayerActive` is `activePlayer == This`.
             // So "Active Timer" logic applies if `isActive && isThisPlayerActive`.
             
             // Let's rewrite to match original exactly.
        }
        
        // Re-implement logic carefully
        bool thisTimerRunning = isActive && isThisPlayerActive;
        
        if (thisTimerRunning) {
             if (isLow) {
                Fl_Color colorA = isBlackPlayer ? FL_BLACK : FL_WHITE;
                Fl_Color colorB = isBlackPlayer ? fl_rgb_color(40, 40, 40) : fl_rgb_color(220, 220, 220);
                color = flash ? colorA : colorB;
                labelColor = (isBlackPlayer ? FL_WHITE : FL_BLACK);
             } else {
                color = isBlackPlayer ? FL_BLACK : FL_WHITE;
                labelColor = (isBlackPlayer ? FL_WHITE : FL_BLACK);
             }
        } else if (isLow) {
            // Not running but low (paused or opponent turn or game over)
            color = FL_YELLOW;
            labelColor = FL_BLACK;
        }
        
        block.color(color);
        block.labelcolor(labelColor);
        block.redraw();
    };

    setBlock(timerBlackBlock, "Black", blackTime, activePlayer == Player::Black, true);
    setBlock(timerWhiteBlock, "White", whiteTime, activePlayer == Player::White, false);
}

void SidePanel::updateBotSideControl(bool active) {
    if (active) ddBotSide.activate();
    else ddBotSide.deactivate();
}

void SidePanel::updateModeDescription(int strengthIndex) {
    std::string desc = "";
    if (strengthIndex == 0) desc = "Decides how long to think";
    else if (strengthIndex == 1) desc = "Answers right away";
    else if (strengthIndex == 2) desc = "Thinks longer for better moves";
    else if (strengthIndex == 3) desc = "Thinks much longer for stronger moves";
    else desc = "Uses maximum thinking for the best play";
    
    txtModeDesc.label(desc.c_str());
    txtModeDesc.redraw();
}

void SidePanel::updateStats(const std::string& text) {
    txtStats.label(text.c_str());
    txtStats.redraw();
}

void SidePanel::setWinRate(double rate) {
    winRateBar.setWinRate(rate);
}

void SidePanel::setProgress(double progress) {
    progressBar.setProgress(progress);
}

