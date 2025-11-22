#include "components.h"

WinRateBar::WinRateBar(int x, int y, int w, int h) : Fl_Box(x, y, w, h, "") {
    winRate = 50.0;
    box(FL_FLAT_BOX);
}

void WinRateBar::setWinRate(double rate) {
    winRate = rate;
    if (winRate < 0) winRate = 0;
    if (winRate > 100) winRate = 100;
    redraw();
}

void WinRateBar::draw() {
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

ProgressBar::ProgressBar(int x, int y, int w, int h) : Fl_Box(x, y, w, h, "") {
    progress = 0.0;
    box(FL_FLAT_BOX);
}

void ProgressBar::setProgress(double p) {
    if (p < 0) p = 0;
    if (p > 1.0) p = 1.0;
    progress = p;
    redraw();
}

void ProgressBar::draw() {
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
