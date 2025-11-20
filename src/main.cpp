#include "bobcat_ui/all.h"
#include "game/game.h"
#include "ai/bot.h"
#include "ui/gomoku_canvas.h"
#include "ai/benchmark.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <FL/Enumerations.H>
#include <FL/fl_draw.H>

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
    
    // Run Benchmark at start
    std::cout << "Running Benchmark..." << std::endl;
    int sps = Benchmark::run(bot);
    std::cout << "Benchmark Result: " << sps << " SPS" << std::endl;

    GomokuCanvas canvas(0, 0, 600, 600, "", &game);
    
    int panelX = 625; // Centered in the 250px sidebar (600-850)
    int y = 20;
    
    // Turn Indicator
    bobcat::TextBox txtTurn(panelX, y, 200, 40, "Ready");
    txtTurn.box(FL_FLAT_BOX);
    txtTurn.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    txtTurn.color(FL_GREEN);
    txtTurn.labelcolor(FL_BLACK);
    y += 60;

    bobcat::Button btnNewGame(panelX, y, 200, 30, "New Game");
    y += 50;
    
    bobcat::Button btnUndo(panelX, y, 200, 30, "Undo");
    y += 50;
    
    bobcat::Dropdown ddMode(panelX, y, 200, 30, "Game Mode");
    ddMode.add("Human vs Bot");
    ddMode.add("Human vs Human");
    ddMode.value(0); 
    y += 50;

    bobcat::Dropdown ddBotSide(panelX, y, 200, 30, "Bot Side");
    ddBotSide.add("Bot: White");
    ddBotSide.add("Bot: Black");
    ddBotSide.value(0);
    y += 50;
    
    bobcat::Dropdown ddBotStrength(panelX, y, 200, 30, "DeepFive Mode");
    ddBotStrength.add("Auto");
    ddBotStrength.add("Instant");
    ddBotStrength.add("Thinking");
    ddBotStrength.add("Extended Thinking");
    ddBotStrength.add("Pro");
    
    // Description Text Box
    bobcat::TextBox txtModeDesc(panelX, y + 40, 200, 60, "Decides how long to think");
    txtModeDesc.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);
    txtModeDesc.labelsize(12);
    txtModeDesc.labelcolor(FL_GRAY0);
    
    if (sps > 10000) {
        ddBotStrength.value(2); // Thinking
        bot.setMode(BotMode::Thinking);
        txtModeDesc.label("Thinks longer for better moves");
    } else {
        ddBotStrength.value(0); // Auto
        bot.setMode(BotMode::Auto);
        txtModeDesc.label("Decides how long to think");
    }
    y += 120;
    
    // Win Rate Bar
    WinRateBar winRateBar(panelX, y, 200, 20);
    y += 40;
    
    // Progress Bar (Hidden when not thinking)
    ProgressBar progressBar(panelX, y, 200, 10);
    // progressBar.hide(); 
    y += 30;

    // Status Text Box
    std::stringstream initStats;
    initStats << "Benchmark: " << sps << " SPS\n"
              << "Rec. Mode: " << (sps > 10000 ? "Thinking" : "Auto");
    
    bobcat::TextBox txtStats(panelX, y, 220, 120, initStats.str());
    txtStats.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT);
    txtStats.labelcolor(FL_GRAY0);
    y += 130;

    auto formatStats = [&](double winRate, int sims, double elapsedSec) {
        std::stringstream ss;
        ss << "Time: " << std::fixed << std::setprecision(1) << elapsedSec << "s\n"
           << "Sims: " << sims << "\n"
           << "Win Rate: " << std::fixed << std::setprecision(1) << winRate << "%\n";
        return ss.str();
    };

    std::string lastStats = initStats.str();

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

    auto tryBotPlay = [&]() {
        if (game.isBotTurn()) {
            updateTitle(window, game);
            updateTurnIndicator(txtTurn, game);
            
            // Show progress bar
            progressBar.setProgress(0.0);
            // progressBar.show();
            
            Fl::check();
            
            bot.setSearchCallback([&](double winRate, int sims, double elapsedSec, double progress) {
                std::string currentStats = "Bot Thinking...\n" + formatStats(winRate, sims, elapsedSec);
                txtStats.label(currentStats);
                txtStats.redraw(); 
                
                progressBar.setProgress(progress);
                
                lastStats = "Last Move:\n" + formatStats(winRate, sims, elapsedSec);
            });

            if (game.playBotMove(bot)) {
                canvas.redraw();
                updateTitle(window, game);
                updateTurnIndicator(txtTurn, game);
                txtStats.label(lastStats);
                txtStats.redraw();
                
                // Hide progress bar
                progressBar.setProgress(0.0);
                
                // Bot moved, start analysis for Human's turn
                startBackgroundAnalysis();
            }
        }
    };

    btnNewGame.onClick([&](bobcat::Widget* w) {
        analysisBot.stopAnalysis();
        game.reset();
        
        GameMode m = (ddMode.value() == 0) ? GameMode::HumanVsBot : GameMode::HumanVsHuman;
        game.setMode(m);
        
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
        game.setBotSide(botSide);
        
        txtStats.label("New Game Started");
        lastStats = "";
        winRateBar.setWinRate(50.0);
        progressBar.setProgress(0.0);
        
        canvas.redraw();
        updateTitle(window, game);
        updateTurnIndicator(txtTurn, game);
        
        startBackgroundAnalysis();
        tryBotPlay();
    });

    btnUndo.onClick([&](bobcat::Widget* w) {
        if (game.canUndo()) {
            analysisBot.stopAnalysis();
            game.undoLastMove();
            
            if (ddMode.value() == 0) {
                if (game.isBotTurn() && game.canUndo()) {
                    game.undoLastMove();
                }
            }
            
            canvas.redraw();
            updateTitle(window, game);
            updateTurnIndicator(txtTurn, game);
            txtStats.label("Undo performed");
            progressBar.setProgress(0.0);
            
            startBackgroundAnalysis();
        }
    });

    ddMode.onChange([&](bobcat::Widget* w) {
        analysisBot.stopAnalysis();
        GameMode m = (ddMode.value() == 0) ? GameMode::HumanVsBot : GameMode::HumanVsHuman;
        game.setMode(m);
        updateTitle(window, game);
        updateTurnIndicator(txtTurn, game);
        
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
        analysisBot.stopAnalysis();
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
        game.setBotSide(botSide);
        canvas.redraw();
        updateTitle(window, game);
        updateTurnIndicator(txtTurn, game);
        
        startBackgroundAnalysis();
        tryBotPlay();
    });

    canvas.onMove([&]() {
        analysisBot.stopAnalysis(); // Stop old analysis
        updateTitle(window, game);
        updateTurnIndicator(txtTurn, game);
        
        startBackgroundAnalysis(); // Start new one
        tryBotPlay();
    });
    
    updateTitle(window, game);
    updateTurnIndicator(txtTurn, game);
    startBackgroundAnalysis(); // Initial analysis

    window.show();
    Fl::lock(); // Enable thread support
    int exitCode = Fl::run();
    analysisBot.stopAnalysis();
    return exitCode;
}
