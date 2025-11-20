#include "bobcat_ui/all.h"
#include "game.h"
#include "bot.h"
#include "gomoku_canvas.h"
#include <iostream>
#include <string>

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
    GomokuBot bot(BotDifficulty::Normal);

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
    
    bobcat::Dropdown ddDiff(panelX, y, 200, 30, "Bot Difficulty");
    ddDiff.add("Easy");
    ddDiff.add("Normal");
    ddDiff.add("Hard");
    ddDiff.value(1);
    y += 60;
    
    bobcat::Dropdown ddBotSide(panelX, y, 200, 30, "Bot Side");
    ddBotSide.add("Bot: White");
    ddBotSide.add("Bot: Black");
    ddBotSide.value(0);
    y += 50;

    auto tryBotPlay = [&]() {
        if (game.isBotTurn()) {
            updateTitle(window, game);
            Fl::check();
            
            if (game.playBotMove(bot)) {
                canvas.redraw();
                updateTitle(window, game);
            }
        }
    };

    btnNewGame.onClick([&](bobcat::Widget* w) {
        game.reset();
        
        GameMode m = (ddMode.value() == 0) ? GameMode::HumanVsBot : GameMode::HumanVsHuman;
        game.setMode(m);
        
        Player botSide = (ddBotSide.value() == 0) ? Player::White : Player::Black;
        game.setBotSide(botSide);
        
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
        }
    });

    ddMode.onChange([&](bobcat::Widget* w) {
        GameMode m = (ddMode.value() == 0) ? GameMode::HumanVsBot : GameMode::HumanVsHuman;
        game.setMode(m);
        updateTitle(window, game);
        tryBotPlay();
    });

    ddDiff.onChange([&](bobcat::Widget* w) {
        int v = ddDiff.value();
        if (v == 0) bot.setDifficulty(BotDifficulty::Easy);
        else if (v == 1) bot.setDifficulty(BotDifficulty::Normal);
        else bot.setDifficulty(BotDifficulty::Hard);
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
