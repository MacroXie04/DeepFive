#include "gomoku_canvas.h"

#include <cmath>
#include <iostream>

GomokuCanvas::GomokuCanvas(int x, int y, int w, int h, std::string title, GomokuGame* game)
    : bobcat::Canvas_(x, y, w, h, title), game(game), moveCb(nullptr) {
    onMouseDown([this](bobcat::Widget* w, float x, float y) {
        int row, col;
        if (this->pixelToCell(x, y, row, col)) {
            if (this->game->playHumanMove(row, col)) {
                this->redraw();
                if (this->moveCb) this->moveCb();
            }
        }
    });
}

void GomokuCanvas::setGame(GomokuGame* game) {
    this->game = game;
}

void GomokuCanvas::onMove(std::function<void()> cb) {
    this->moveCb = cb;
}

void GomokuCanvas::setPreviewMoves(const std::vector<PreviewMove>& moves) {
    previewMoves = moves;
    redraw();
}

void GomokuCanvas::clearPreviewMoves() {
    previewMoves.clear();
    redraw();
}

void GomokuCanvas::draw() {
    if (!valid()) {
        valid(1);
        glViewport(0, 0, w(), h());
    }

    // Background color - wood texture style
    glClearColor(0.87f, 0.72f, 0.53f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glPointSize(7.0f);

    render();

    // Do NOT call swap_buffers() manually, let FLTK handle it.
}

void GomokuCanvas::render() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if (w() > h()) {
        scaleX = (float)h() / w();
    } else {
        scaleY = (float)w() / h();
    }

    float boardLimit = 0.9f;

    glLineWidth(1.0f);
    glColor3f(0.0f, 0.0f, 0.0f);

    int gridSize = game->getBoard().size();
    float step = (2.0f * boardLimit) / (gridSize - 1);

    glBegin(GL_LINES);
    for (int i = 0; i < gridSize; ++i) {
        float y = -boardLimit + i * step;
        glVertex2f(-boardLimit * scaleX, y * scaleY);
        glVertex2f(boardLimit * scaleX, y * scaleY);
    }
    for (int i = 0; i < gridSize; ++i) {
        float x = -boardLimit + i * step;
        glVertex2f(x * scaleX, -boardLimit * scaleY);
        glVertex2f(x * scaleX, boardLimit * scaleY);
    }
    glEnd();

    drawPreviewMoves();
    drawStones();
}

void GomokuCanvas::drawStones() {
    float boardLimit = 0.9f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (w() > h())
        scaleX = (float)h() / w();
    else
        scaleY = (float)w() / h();

    int gridSize = game->getBoard().size();
    float step = (2.0f * boardLimit) / (gridSize - 1);
    float radius = step * 0.4f;

    const Board& b = game->getBoard();
    for (int r = 0; r < gridSize; ++r) {
        for (int c = 0; c < gridSize; ++c) {
            Player p = b.at(r, c);
            if (p != Player::NoPlayer) {
                float cx = (-boardLimit + c * step) * scaleX;
                float cy = (boardLimit - r * step) * scaleY;

                glBegin(GL_TRIANGLE_FAN);
                if (p == Player::Black)
                    glColor3f(0.0f, 0.0f, 0.0f);
                else
                    glColor3f(1.0f, 1.0f, 1.0f);

                glVertex2f(cx, cy);
                for (int i = 0; i <= 20; ++i) {
                    float angle = 2.0f * 3.14159f * i / 20.0f;
                    glVertex2f(cx + radius * cos(angle) * scaleX,
                               cy + radius * sin(angle) * scaleY);
                }
                glEnd();

                if (p == Player::White) {
                    glBegin(GL_LINE_LOOP);
                    glColor3f(0.0f, 0.0f, 0.0f);
                    for (int i = 0; i <= 20; ++i) {
                        float angle = 2.0f * 3.14159f * i / 20.0f;
                        glVertex2f(cx + radius * cos(angle) * scaleX,
                                   cy + radius * sin(angle) * scaleY);
                    }
                    glEnd();
                }
            }
        }
    }

    // Draw winning line highlight
    const auto& winningLine = game->getWinningLine();
    if (!winningLine.empty()) {
        glLineWidth(3.0f);
        glColor3f(1.0f, 0.0f, 0.0f);  // Red color for highlight

        for (const auto& pos : winningLine) {
            int r = pos.first;
            int c = pos.second;
            float cx = (-boardLimit + c * step) * scaleX;
            float cy = (boardLimit - r * step) * scaleY;
            float boxSize = radius * 1.3f;

            // Draw a square around the winning stone
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - boxSize * scaleX, cy - boxSize * scaleY);
            glVertex2f(cx + boxSize * scaleX, cy - boxSize * scaleY);
            glVertex2f(cx + boxSize * scaleX, cy + boxSize * scaleY);
            glVertex2f(cx - boxSize * scaleX, cy + boxSize * scaleY);
            glEnd();
        }

        glLineWidth(1.0f);  // Reset line width
    }
}

bool GomokuCanvas::pixelToCell(float x, float y, int& row, int& col) const {
    float boardLimit = 0.9f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (w() > h())
        scaleX = (float)h() / w();
    else
        scaleY = (float)w() / h();

    float ux = x / scaleX;
    float uy = y / scaleY;

    if (ux < -boardLimit || ux > boardLimit || uy < -boardLimit || uy > boardLimit) {
        return false;
    }

    int gridSize = game->getBoard().size();
    float step = (2.0f * boardLimit) / (gridSize - 1);

    float fCol = (ux + boardLimit) / step;
    float fRow = (boardLimit - uy) / step;

    col = (int)round(fCol);
    row = (int)round(fRow);

    if (col < 0 || col >= gridSize || row < 0 || row >= gridSize) return false;

    return true;
}

void GomokuCanvas::drawDashedCircle(float cx, float cy, float radius, float scaleX, float scaleY) {
    const int dashCount = 16;  // Number of dashes
    
    glBegin(GL_LINES);
    for (int d = 0; d < dashCount; ++d) {
        // Draw every other segment (dashed effect)
        float startAngle = 2.0f * 3.14159f * d / dashCount;
        float endAngle = 2.0f * 3.14159f * (d + 0.5f) / dashCount;
        
        float x1 = cx + radius * cos(startAngle) * scaleX;
        float y1 = cy + radius * sin(startAngle) * scaleY;
        float x2 = cx + radius * cos(endAngle) * scaleX;
        float y2 = cy + radius * sin(endAngle) * scaleY;
        
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
}

void GomokuCanvas::drawPreviewMoves() {
    if (previewMoves.empty()) return;

    float boardLimit = 0.9f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (w() > h())
        scaleX = (float)h() / w();
    else
        scaleY = (float)w() / h();

    int gridSize = game->getBoard().size();
    float step = (2.0f * boardLimit) / (gridSize - 1);
    float radius = step * 0.4f;

    for (const auto& pm : previewMoves) {
        // Skip if there's already a stone at this position
        if (game->getBoard().at(pm.row, pm.col) != Player::NoPlayer) continue;

        float cx = (-boardLimit + pm.col * step) * scaleX;
        float cy = (boardLimit - pm.row * step) * scaleY;

        // Calculate alpha based on score (0.15 to 0.7 range)
        float alpha = 0.15f + pm.score * 0.55f;

        // Draw filled circle with transparency (evaluation indicator)
        glBegin(GL_TRIANGLE_FAN);
        if (pm.player == Player::Black)
            glColor4f(0.0f, 0.0f, 0.0f, alpha);
        else
            glColor4f(0.9f, 0.9f, 0.9f, alpha);

        glVertex2f(cx, cy);
        for (int i = 0; i <= 20; ++i) {
            float angle = 2.0f * 3.14159f * i / 20.0f;
            glVertex2f(cx + radius * 0.85f * cos(angle) * scaleX,
                       cy + radius * 0.85f * sin(angle) * scaleY);
        }
        glEnd();

        // Draw dashed border (hollow stone effect)
        glLineWidth(2.0f);
        if (pm.player == Player::Black)
            glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
        else
            glColor4f(0.3f, 0.3f, 0.3f, 0.8f);
        
        drawDashedCircle(cx, cy, radius, scaleX, scaleY);
        glLineWidth(1.0f);
    }
}
