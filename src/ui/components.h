#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <cstdio>

class WinRateBar : public Fl_Box {
    double winRate; // Black's win rate (0-100)
public:
    WinRateBar(int x, int y, int w, int h);
    void setWinRate(double rate);
    void draw() override;
};

class ProgressBar : public Fl_Box {
    double progress; // 0.0 to 1.0
public:
    ProgressBar(int x, int y, int w, int h);
    void setProgress(double p);
    void draw() override;
};

#endif
