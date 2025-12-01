#ifndef GOMOKU_CANVAS_H
#define GOMOKU_CANVAS_H

#include "bobcat_ui/canvas.h"

// Fix X11 None macro conflict
#ifdef None
#undef None
#endif

#include <functional>
#include <vector>

#include "../game/game.h"

// Preview move with evaluation score (0.0 to 1.0)
struct PreviewMove {
    int row;
    int col;
    Player player;
    float score;  // 0.0 = worst, 1.0 = best
};

class GomokuCanvas : public bobcat::Canvas_ {
   public:
    GomokuCanvas(int x, int y, int w, int h, std::string title, GomokuGame* game);

    void setGame(GomokuGame* game);
    void render() override;
    void draw() override;
    void onMove(std::function<void()> cb);

    // Preview moves for bot thinking visualization
    void setPreviewMoves(const std::vector<PreviewMove>& moves);
    void clearPreviewMoves();

   private:
    GomokuGame* game;
    float cellSize;
    float boardSize;
    float offsetX;
    float offsetY;
    std::function<void()> moveCb;
    std::vector<PreviewMove> previewMoves;

    bool pixelToCell(float x, float y, int& row, int& col) const;
    void drawGrid();
    void drawStones();
    void drawStone(int row, int col, Player p);
    void drawPreviewMoves();
    void drawDashedCircle(float cx, float cy, float radius, float scaleX, float scaleY);
};

#endif
