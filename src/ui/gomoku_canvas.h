#ifndef GOMOKU_CANVAS_H
#define GOMOKU_CANVAS_H

#include "bobcat_ui/canvas.h"
#include "../game/game.h"
#include <functional>

class GomokuCanvas : public bobcat::Canvas_ {
public:
    GomokuCanvas(int x, int y, int w, int h, std::string title, GomokuGame* game);

    void setGame(GomokuGame* game);
    void render() override;
    void draw() override;
    void onMove(std::function<void()> cb);

private:
    GomokuGame* game;
    float cellSize;
    float boardSize;
    float offsetX;
    float offsetY;
    std::function<void()> moveCb;

    bool pixelToCell(float x, float y, int& row, int& col) const;
    void drawGrid();
    void drawStones();
    void drawStone(int row, int col, Player p);
};

#endif
