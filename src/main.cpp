#include "bobcat_ui/all.h"
#include "game/game.h"
#include "ai/bot.h"
#include "ui/gomoku_canvas.h"
#include "ai/benchmark.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

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

int main(int argc, char **argv) {
    bobcat::Window window(850, 600, "DeepFive Gomoku");

    GomokuGame game(15);
    GomokuBot bot;
    
    // Run Benchmark at start
    // TODO: Show a splash screen? For now, print to console and set default
    std::cout << "Running Benchmark..." << std::endl;
    int sps = Benchmark::run(bot);
    std::cout << "Benchmark Result: " << sps << " SPS" << std::endl;

    GomokuCanvas canvas(0, 0, 600, 600, "", &game);
    
    int panelX = 610;
    int y = 20;
    
    bobcat::Button btnNewGame(panelX, y, 200, 30, "New Game");
    y += 50;
    
    bobcat::Button btnUndo(panelX, y, 200, 30, "Undo");
    y += 60;
    
    bobcat::Dropdown ddMode(panelX, y, 200, 30, "Game Mode");
    ddMode.add("Human vs Bot");
    ddMode.add("Human vs Human");
    ddMode.value(0); 
    y += 60;
    
    bobcat::Dropdown ddBotStrength(panelX, y, 200, 30, "Bot Strength");
    ddBotStrength.add("Instant (1s)");
    ddBotStrength.add("Auto (Dynamic)");
    ddBotStrength.add("Thinking (10s)");
    ddBotStrength.add("Extended Thinking (30s)");
    
    // Determine default strength based on Benchmark
    if (sps > 10000) {
        ddBotStrength.value(2); // Thinking
        bot.setMode(BotMode::Thinking);
    } else {
        ddBotStrength.value(1); // Auto
        bot.setMode(BotMode::Auto);
    }
    y += 60;
    
    bobcat::Dropdown ddBotSide(panelX, y, 200, 30, "Bot Side");
    ddBotSide.add("Bot: White");
    ddBotSide.add("Bot: Black");
    ddBotSide.value(0);
    y += 50;

    // Status Text Box
    std::stringstream initStats;
    initStats << "Benchmark: " << sps << " SPS\n"
              << "Rec. Mode: " << (sps > 10000 ? "Thinking" : "Auto");
    
    bobcat::TextBox txtStats(panelX, y, 220, 120, initStats.str());
    txtStats.align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT);
    txtStats.labelcolor(FL_GRAY0);
    y += 130;

    // Helper to format stats
    auto formatStats = [&](double winRate, int sims, double elapsedSec) {
        std::stringstream ss;
        ss << "Time: " << std::fixed << std::setprecision(1) << elapsedSec << "s\n"
           << "Sims: " << sims << "\n"
           << "Win Rate: " << std::fixed << std::setprecision(1) << winRate << "%\n";
        return ss.str();
    };

    std::string lastStats = initStats.str();

    auto tryBotPlay = [&]() {
        if (game.isBotTurn()) {
            updateTitle(window, game);
            Fl::check();
            
            bot.setSearchCallback([&](double winRate, int sims, double elapsedSec) {
                std::string currentStats = "Thinking...\n" + formatStats(winRate, sims, elapsedSec);
                txtStats.label(currentStats);
                txtStats.redraw(); 
                
                lastStats = "Last Move:\n" + formatStats(winRate, sims, elapsedSec);
            });

            if (game.playBotMove(bot)) {
                canvas.redraw();
                updateTitle(window, game);
                txtStats.label(lastStats);
                txtStats.redraw();
            }
        }
    };

    btnNewGame.onClick([&](bobcat::Widget* w) {
        game.reset();
        
        GameMode m = (ddMode.value() == 0) ? GameMode::HumanVsBot : GameMode::HumanVsHuman;
        game.setMode(m);
        
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
        game.setBotSide(botSide);
        
        // Reset stats but keep benchmark info? No, usually clear it.
        txtStats.label("New Game Started");
        lastStats = "";
        
        canvas.redraw();
        updateTitle(window, game);
        
        tryBotPlay();
    });

    btnUndo.onClick([&](bobcat::Widget* w) {
        if (game.canUndo()) {
            game.undoLastMove();
            
            if (ddMode.value() == 0) {
                if (game.isBotTurn() && game.canUndo()) {
                    game.undoLastMove();
                }
            }
            
            canvas.redraw();
            updateTitle(window, game);
            txtStats.label("Undo performed");
        }
    });

    ddMode.onChange([&](bobcat::Widget* w) {
        GameMode m = (ddMode.value() == 0) ? GameMode::HumanVsBot : GameMode::HumanVsHuman;
        game.setMode(m);
        updateTitle(window, game);
        tryBotPlay();
    });
    
    ddBotStrength.onChange([&](bobcat::Widget* w) {
        int v = ddBotStrength.value();
        if (v == 0) bot.setMode(BotMode::Instant);
        else if (v == 1) bot.setMode(BotMode::Auto);
        else if (v == 2) bot.setMode(BotMode::Thinking);
        else bot.setMode(BotMode::ExtendedThinking);
    });

    ddBotSide.onChange([&](bobcat::Widget* w) {
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
        game.setBotSide(botSide);
        canvas.redraw();
        updateTitle(window, game);
        tryBotPlay();
    });

    canvas.onMove([&]() {
        updateTitle(window, game);
        tryBotPlay();
    });
    
    updateTitle(window, game);

    window.show();
    return Fl::run();
}
