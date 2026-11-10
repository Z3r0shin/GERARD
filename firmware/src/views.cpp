// SPDX-License-Identifier: GPL-3.0-or-later
// GERARD firmware: the run views, the animations shown in place of the count during a run.
//
// Ported line for line from the script of the owner's mockup (gerard-run-views.html, with its
// brief README.md, 2026-11-06), so the pixels match the mockup: the drawing helpers below are the
// mockup's own (px rounds each coordinate to the nearest pixel, halves up, and drops pixels off the
// screen), in double precision as in JavaScript. Every function keeps the mockup's name.
//
// The art was drawn for GERARD as mockups, at the owner's request, by Claude in a brainstorm
// session on 2026-11-06; nothing is copied from another work. It is under the firmware's licence,
// GPL-3.0-or-later (the owner, 2026-11-06: "3- yes").
//
// The buffer holds rows 16 to 63 only, in the display's page layout (6 pages of 128 bytes, each
// byte 8 rows, top row in bit 0), so a view can never touch the top line.

#include "views.h"

#include <Arduino.h>
#include <pico/time.h>
#include <math.h>
#include <string.h>

#include <algorithm>
#include <vector>

namespace {

constexpr int W = 128, H = 64, TOP = 16;
constexpr double PI_ = 3.14159265358979323846;
constexpr double MIN_MS = 60000.0;

uint8_t vb[6 * 128];
bool maskOn = false;  // masked(): draw only where x + y is even
uint32_t lastUs = 0, maxUs = 0;
long frames = 0;  // frames drawn, the mockup's frame counter (the hourglass's stream)

inline int rnd(double v) { return (int)floor(v + 0.5); }  // JavaScript's Math.round
inline double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// ---------- 1-bit drawing, as in the mockup ----------
void px(double xd, double yd) {
    int x = rnd(xd), y = rnd(yd);
    if (x < 0 || y < TOP || x >= W || y >= H) return;
    if (maskOn && ((x + y) & 1)) return;
    vb[((y >> 3) - 2) * W + x] |= (uint8_t)(1u << (y & 7));
}
void off(double xd, double yd) {
    int x = rnd(xd), y = rnd(yd);
    if (x < 0 || y < TOP || x >= W || y >= H) return;
    vb[((y >> 3) - 2) * W + x] &= (uint8_t)~(1u << (y & 7));
}
void hline(double x0, double x1, double y) { for (double x = x0; x <= x1; x += 1) px(x, y); }
void vline(double x, double y0, double y1) { for (double y = y0; y <= y1; y += 1) px(x, y); }
void frect(double x, double y, double w, double h) { for (int j = 0; j < h; j++) hline(x, x + w - 1, y + j); }
void rect(double x, double y, double w, double h) {
    hline(x, x + w - 1, y); hline(x, x + w - 1, y + h - 1); vline(x, y, y + h - 1); vline(x + w - 1, y, y + h - 1);
}
void line(double ax, double ay, double bx, double by) {
    int x0 = rnd(ax), y0 = rnd(ay), x1 = rnd(bx), y1 = rnd(by);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int e = dx + dy;
    for (;;) {
        px(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
void tline(double x0, double y0, double x1, double y1, int w) { for (int i = 0; i < w; i++) line(x0 + i, y0, x1 + i, y1); }
void circle(double cx, double cy, int r) {
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
        px(cx + x, cy + y); px(cx + y, cy + x); px(cx - y, cy + x); px(cx - x, cy + y);
        px(cx - x, cy - y); px(cx - y, cy - x); px(cx + y, cy - x); px(cx + x, cy - y);
        y++;
        if (e < 0) e += 2 * y + 1; else { x--; e += 2 * (y - x) + 1; }
    }
}
void fcircle(double cx, double cy, int r) {
    for (int dy = -r; dy <= r; dy++) {
        double dx = floor(sqrt((double)(r * r - dy * dy)) + 0.3);
        hline(cx - dx, cx + dx, cy + dy);
    }
}
template <size_t N>
void bmp(const char *const (&rows)[N], double x, double y, bool flip) {
    for (size_t j = 0; j < N; j++)
        for (size_t i = 0; rows[j][i]; i++)
            if (rows[j][i] == '#') px(flip ? x - i : x + i, y + j);
}
void crect(double x, double y, int w, int h) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) off(x + i, y + j); }
void curve(double x0, double y0, double cx1, double cy1, double x1, double y1) {
    double lx = x0, ly = y0;
    for (int i = 1; i <= 12; i++) {
        double u = i / 12.0, a = (1 - u) * (1 - u), b = 2 * u * (1 - u), c = u * u;
        double x = a * x0 + b * cx1 + c * x1, y = a * y0 + b * cy1 + c * y1;
        line(lx, ly, x, y);
        lx = x; ly = y;
    }
}
void routeBar(double p) {
    for (int x = 4; x <= 123; x += 2) px(x, 18);
    for (int k = 0; k <= 3; k++) vline(rnd(4 + k * 119 / 3.0), 17, 19);
    frect(rnd(4 + clamp01(p) * 119) - 1, 17, 3, 3);
}
void bottomBar(double p) {
    for (int x = 0; x < 128; x += 4) px(x, 63);
    int end = rnd(clamp01(p) * 127);
    if (end > 0) { hline(0, end, 62); hline(0, end, 63); }
}
uint32_t rng = 0x2545F491u;
double rand01() {  // any cheap generator will do (the robot's sparks)
    rng = rng * 1664525u + 1013904223u;
    return (rng >> 8) / 16777216.0;
}

// ---------- view A: hourglass ----------
struct Grain { int16_t x, y; double k; };
constexpr int HG_CX = 64, HG_TOP = 19, HG_NECK = 40, HG_BOT = 61;
std::vector<Grain> hgTop, hgBot;
int hgW(double t) { return rnd(1 + 10 * sin(t * PI_ / 2)); }
void hourglassBegin() {
    for (int y = HG_TOP; y < HG_NECK; y++) {
        int w = hgW((double)(HG_NECK - y) / (HG_NECK - HG_TOP));
        if (y >= HG_TOP + 3)
            for (int dx = -(w - 1); dx <= w - 1; dx++)
                hgTop.push_back({(int16_t)(HG_CX + dx), (int16_t)y, (HG_NECK - y) - abs(dx) * 0.12});
    }
    for (int y = HG_NECK + 1; y <= HG_BOT; y++) {
        int w = hgW((double)(y - HG_NECK) / (HG_BOT - HG_NECK));
        for (int dx = -(w - 1); dx <= w - 1; dx++)
            hgBot.push_back({(int16_t)(HG_CX + dx), (int16_t)y, (HG_BOT - y) + abs(dx) * 0.5});
    }
    // JavaScript's sort is stable: equal keys keep their order.
    std::stable_sort(hgTop.begin(), hgTop.end(), [](const Grain &a, const Grain &b) { return b.k < a.k; });
    std::stable_sort(hgBot.begin(), hgBot.end(), [](const Grain &a, const Grain &b) { return a.k < b.k; });
}
void drawHourglass(double p, long frame) {
    const int cx = HG_CX, grains = (int)hgTop.size();
    frect(cx - 16, 17, 33, 2); frect(cx - 16, 62, 33, 2);
    vline(cx - 14, 19, 61); vline(cx + 14, 19, 61);
    for (int y = HG_TOP; y <= HG_BOT; y++) {
        double t = y <= HG_NECK ? (double)(HG_NECK - y) / (HG_NECK - HG_TOP) : (double)(y - HG_NECK) / (HG_BOT - HG_NECK);
        int w = hgW(t);
        px(cx - w, y); px(cx + w, y);
    }
    int moved = rnd(clamp01(p) * grains);
    for (int i = moved; i < grains; i++) px(hgTop[i].x, hgTop[i].y);
    int pileTop = HG_BOT + 1;
    for (int i = 0; i < moved; i++) {
        const Grain &g = hgBot[i];
        px(g.x, g.y);
        if (g.x == cx && g.y < pileTop) pileTop = g.y;
    }
    if (moved > 0 && moved < grains) {
        px(cx, HG_NECK);
        for (int y = HG_NECK + 1; y < pileTop; y++)
            if ((((y - frame) % 3) + 3) % 3 == 0) px(cx, y);
    }
}

// ---------- view B: plant ----------
const char *const LEAF_BIG[] = {"...##", ".####", "###.."};
const char *const LEAF_SMALL[] = {".#", "##"};
const char *const FLOWER[] = {"..#..", ".###.", "##.##", ".###.", "..#.."};
void drawPlant(double p, double t, uint32_t breaks) {
    const int cx = 64;
    frect(cx - 13, 52, 27, 3);
    for (int y = 55; y <= 63; y++) {
        int w = rnd(11 - (y - 55) * 3 / 8.0);
        px(cx - w, y); px(cx + w, y);
    }
    hline(cx - 8, cx + 8, 63);
    for (int x = cx - 8; x <= cx + 8; x += 2) px(x, 58);
    int Hs = 3 + rnd(fmin(1.0, p) * 26);
    int ptx[32], pty[32];
    for (int h = 0; h <= Hs; h++) {
        double f = (double)h / (Hs > 1 ? Hs : 1);
        double sway = sin(t * 1.05) * 1.7 * f * f + sin(h * 0.35) * 0.6 * f;
        int x = rnd(cx + sway), y = 51 - h;
        px(x, y);
        ptx[h] = x; pty[h] = y;
    }
    const int LH[] = {5, 10, 15, 20};
    for (int i = 0; i < 4; i++) {
        int lh = LH[i];
        if (Hs >= lh + 3) {
            int x = ptx[lh], y = pty[lh];
            bool left = i % 2 == 1;
            bool big = Hs - lh > 8;
            if (big) bmp(LEAF_BIG, left ? x - 1 : x + 1, y - 3 + 1, left);
            else bmp(LEAF_SMALL, left ? x - 1 : x + 1, y - 2 + 1, left);
        }
    }
    int tx = ptx[Hs], ty = pty[Hs];
    if (p >= 0.9) bmp(FLOWER, tx - 2, ty - 5, false);
    else if (p >= 0.75) { px(tx, ty - 1); px(tx - 1, ty - 1); px(tx + 1, ty - 1); px(tx, ty - 2); }
    uint32_t n = breaks < 7 ? breaks : 7;
    for (uint32_t i = 0; i < n; i++) {
        int x = 6 + i * 7;
        vline(x, 61, 63); px(x, 59); px(x - 1, 60); px(x + 1, 60); px(x, 60);
    }
    double per = 25, phase = fmod(t, per);
    if (phase < 8) {
        double bx = 132 - phase * 17, by = 30 + sin(bx * 0.13) * 6;
        if ((long long)floor(t * 6) % 2) { px(bx - 1, by); px(bx + 1, by); px(bx, by + 1); }
        else { px(bx, by); px(bx, by + 1); }
    }
}

// ---------- view E: coffee pot ----------
int coffeeBw(int y) { return rnd(15 - pow((y - 47) / 11.0, 4) * 3); }
void drawCoffee(double p, double t) {
    p = clamp01(p);
    const int cx = 60;
    frect(22, 17, 8, 43); frect(22, 17, 62, 4); frect(18, 60, 72, 4);
    if ((long long)floor(t * 2) % 2 == 0) crect(80, 61, 2, 2);
    for (int y = 21; y <= 28; y++) {
        int w = rnd(16 - (y - 21) * 1.5);
        px(cx - w, y); px(cx + w, y);
    }
    hline(cx - 5, cx + 5, 28);
    for (int x = cx - 9; x <= cx + 9; x += 4) px(x, 24);
    px(cx, 29);
    hline(cx - 12, cx + 9, 32); px(cx - 13, 31); px(cx - 14, 31);
    for (int y = 33; y <= 36; y++) { px(cx - 9, y); px(cx + 9, y); }
    hline(cx - 12, cx - 9, 37); hline(cx + 9, cx + 12, 37);
    for (int y = 38; y <= 58; y++) { int w = coffeeBw(y); px(cx - w, y); px(cx + w, y); }
    hline(cx - 12, cx + 12, 59);
    hline(cx + 15, cx + 19, 40); vline(cx + 19, 40, 53); hline(cx + 15, cx + 19, 53);
    int top = 58 - rnd(p * 19);
    for (int y = top; y <= 58; y++) {
        int w = coffeeBw(y) - 2;
        for (int x = cx - w; x <= cx + w; x++)
            if (y > top || sin(x * 0.9 + t * 6) > -0.6) px(x, y);
    }
    if (p < 1) {
        double ph = fmod(t, 0.7) / 0.7, dy = 30 + ph * ph * (top - 31);
        px(cx, dy); px(cx, dy + 1);
        if (ph < 0.15) { px(cx - 2, top - 1); px(cx + 2, top - 1); }
    }
    if (p > 0.25) {
        int f = (int)((long long)floor(t * 3) % 2);
        for (int y = 22; y <= 30; y += 2) px(cx - 15 + ((y / 2 + f) % 2), y);
    }
    rect(100, 50, 12, 13); hline(98, 113, 49); hline(112, 115, 53); vline(115, 53, 58); hline(112, 115, 58);
}

// ---------- view F: train ----------
void trainWheel(double x, double a) { circle(x, 56, 2); px(x + rnd(cos(a)), 56 + rnd(sin(a))); }
void drawTrain(double p, double t, double pos, double len) {
    routeBar(p);
    double o1 = t * 2;
    for (int x = 0; x < 128; x++) px(x, rnd(33 + 3 * sin((x + o1) * 0.05) + 2 * sin((x + o1) * 0.13)));
    for (int k = 1; k <= 3; k++) {
        int sx = rnd(84 + (k * len / 3 - pos) / 1000 * 3);
        if (sx < -30 || sx > 160) continue;
        crect(sx - 14, 34, 29, 24);
        line(sx - 14, 41, sx, 34); line(sx, 34, sx + 14, 41);
        rect(sx - 12, 41, 25, 13); rect(sx - 3, 46, 6, 8); rect(sx - 10, 44, 5, 4); rect(sx + 6, 44, 5, 4);
        if (k == 3) { vline(sx + 18, 30, 57); frect(sx + 19, 30, 6, 4); }
    }
    double o2 = fmod(t * 12, 40);
    for (double x = -o2; x < 140; x += 40) { int X = rnd(x); vline(X, 40, 57); hline(X - 3, X + 3, 41); }
    for (double x = -o2; x < 140; x += 40) { int X = rnd(x); curve(X + 3, 41, X + 20, 45, X + 37, 41); }
    hline(0, 127, 59);
    double o3 = fmod(t * 24, 6);
    for (double x = -o3; x < 128; x += 6) { px(x, 60); px(x + 1, 60); px(x, 61); px(x + 1, 61); }
    for (int x = rnd(fmod(-(t * 24), 4)); x < 128; x += 4) px(x, 63);
    double a = t * 7;
    for (int x : {10, 38}) {
        crect(x - 1, 43, 26, 15); rect(x, 44, 24, 10); hline(x, x + 23, 54);
        for (int w = x + 3; w < x + 21; w += 6) rect(w, 46, 4, 4);
        trainWheel(x + 5, a); trainWheel(x + 18, a);
    }
    hline(34, 38, 51); hline(62, 66, 51);
    crect(64, 39, 42, 19);
    hline(65, 79, 41); rect(66, 42, 13, 12); rect(69, 44, 6, 5);
    rect(79, 46, 21, 8); vline(86, 46, 53); vline(93, 46, 53);
    frect(93, 40, 4, 6); hline(92, 97, 40);
    hline(64, 102, 54); line(100, 54, 104, 58);
    trainWheel(69, a); trainWheel(82, a); trainWheel(94, a);
    int rodY = 56 + rnd(sin(a));
    hline(69, 94, rodY);
    for (int i = 0; i < 4; i++) {
        double age = fmod(t + i * 0.6, 2.4), x = 95 - age * 14, y = 37 - age * 6;
        int r = 1 + (int)floor(age);
        if (y - r >= 21) circle(x, y, r);
    }
}

// ---------- view I: scarf ----------
void drawScarf(double p, double t) {
    p = clamp01(p);
    int k = (int)((long long)floor(t * 3) % 2);
    line(40, 19 + k, 82, 30); line(88, 19 - k + 1, 46, 30);
    fcircle(40, 19 + k, 1); fcircle(88, 20 - k, 1);
    int rows = 1 + rnd(p * 14);
    for (int r = 0; r < rows; r++) {
        int y = 28 + r * 2;
        if (r % 8 == 4) { hline(52, 75, y); hline(52, 75, y + 1); continue; }
        for (int c = 0; c < 6; c++) { int x0 = 52 + c * 4; px(x0, y); px(x0 + 3, y); px(x0 + 1, y + 1); px(x0 + 2, y + 1); }
    }
    int end = 28 + rows * 2;
    vline(51, 28, end - 1); vline(76, 28, end - 1);
    int sw = rnd(sin(t * 1.3));
    for (int x = 53; x <= 75; x += 3) line(x, end, x + sw, end + 3);
    int br = rnd(8 - p * 5), bx = 108, by = 63 - br;
    int wob = rnd(sin(t * 2) * 1);
    circle(bx, by, br);
    if (br > 3) {
        curve(bx - br + 1, by, bx + wob, by - br + 2, bx + br - 1, by - 1);
        curve(bx - br + 2, by + 2, bx - wob, by + 1, bx + br - 2, by + 3);
    }
    curve(82, 30, 100, 44 + wob, bx - 1, by - br);
}

// ---------- view J: robot builds a robot ----------
void robotPart(int k, int ox) {
    switch (k) {
        case 0: rect(ox + 8, 53, 22, 8); for (int w : {12, 19, 26}) circle(ox + w, 57, 2); break;
        case 1: rect(ox + 10, 37, 18, 15); break;
        case 2: circle(ox + 19, 44, 3); px(ox + 19, 43); px(ox + 19, 44); break;
        case 3: rect(ox + 5, 39, 4, 11); break;
        case 4: rect(ox + 29, 39, 4, 11); break;
        case 5: vline(ox + 18, 35, 36); vline(ox + 19, 35, 36); rect(ox + 8, 22, 21, 13); break;
        case 6: vline(ox + 18, 18, 21); fcircle(ox + 18, 17, 1); break;
        case 7: frect(ox + 12, 26, 4, 4); frect(ox + 21, 26, 4, 4); hline(ox + 14, ox + 22, 31); break;
    }
}
const int PART_AT[8][2] = {{19, 57}, {19, 44}, {19, 44}, {7, 44}, {31, 44}, {18, 28}, {18, 19}, {18, 28}};
void drawRobot(double p, double t) {
    p = clamp01(p);
    hline(0, 127, 62);
    bool blink = fmod(t, 3.7) < 0.15;
    for (int k : {0, 1, 2, 3, 5, 6}) robotPart(k, 0);
    if (blink) { hline(12, 15, 28); hline(21, 24, 28); } else robotPart(7, 0);
    if ((long long)floor(t * 2) % 2) off(18, 17);
    const int OX = 73;
    int done = (int)fmin(8.0, floor(p / 0.92 * 8));
    for (int k = 0; k < done; k++) robotPart(k, OX);
    if (done < 8) { maskOn = true; robotPart(done, OX); maskOn = false; }
    if (done >= 8 && (long long)floor(t * 2) % 2) { off(OX + 13, 27); off(OX + 22, 27); }
    int part = done < 7 ? done : 7;
    int ax = PART_AT[part][0], ay = PART_AT[part][1];
    double tx = OX + ax + rnd(4 * sin(t * 1.3)), ty = fmax(18.0, ay + rnd(3 * sin(t * 2.1)));
    double sx = 30, sy = 40, ex = (sx + tx) / 2, ey = fmax(18.0, fmin(sy, ty) - 10);
    tline(sx, sy, ex, ey, 2); tline(ex, ey, tx, ty, 2); fcircle(ex, ey, 1); fcircle(tx, ty, 1);
    if (done < 8 && (long long)floor(t * 10) % 3)
        for (int i = 0; i < 4; i++) px(tx + rnd((rand01() - 0.5) * 8), ty + 2 + rnd(rand01() * 4));
}

// ---------- view K: fishing ----------
const char *const FISH[] = {".##..#", "######", ".##..#"};
struct { long k = -1; double start = -1e9; double seen = 0; } fishSt;
void drawFish(double t, double pos) {
    // catches at 9.5, 19.5 and 29.5 min, so the third lands before the break prompt
    long k = (long)floor((pos + 30000) / (10 * MIN_MS));
    double now = t * 1000;
    bool away = now - fishSt.seen > 500;
    fishSt.seen = now;
    if (fishSt.k < 0 || k < fishSt.k || away) fishSt.k = k;
    else if (k > fishSt.k) { fishSt.k = k; fishSt.start = now; }
    double a = (now - fishSt.start) / 3000;
    circle(112, 24, 5);
    for (int i = 0; i < 8; i++) { double an = i * PI_ / 4 + t * 0.2; px(112 + rnd(8 * cos(an)), 24 + rnd(8 * sin(an))); }
    frect(0, 38, 34, 2); off(9, 38); off(18, 38); off(27, 38); vline(4, 40, 63); vline(29, 40, 63);
    for (int x = 34; x < 128; x++) px(x, 46 + rnd(0.7 * sin(x * 0.3 - t * 1.8)));
    for (int x = 34 + rnd(t * 2) % 9; x < 128; x += 9) px(x, 52);
    for (int x = 38 + rnd(t * 1.3) % 11; x < 128; x += 11) px(x, 58);
    int fx = rnd(140 - fmod(t * 8, 170)), fy = 56 + rnd(sin(t) * 2);
    bmp(FISH, fx, fy, false);
    line(6, 30, 8, 37); line(14, 30, 12, 37); hline(8, 12, 37); hline(6, 14, 30);
    long shown = k - (a < 0.75 ? 1 : 0);
    if (shown > 4) shown = 4;
    for (long i = 0; i < shown; i++) { int x = 8 + i * 2; px(x - 1, 27); px(x + 1, 27); px(x, 28); px(x, 29); }
    circle(21, 25, 3); hline(16, 26, 22); frect(18, 19, 7, 3);
    tline(21, 29, 22, 36, 2); hline(22, 31, 36);
    int sw = rnd(sin(t * 1.5));
    line(31, 36, 33 + sw, 44); hline(33 + sw, 35 + sw, 44);
    line(22, 30, 28, 33);
    bool bite = a >= 0 && a < 0.75;
    int tipX = bite ? 62 : 66, tipY = bite ? 25 : 19;
    curve(28, 33, 46, bite ? 24 : 18, tipX, tipY);
    if (a >= 0.3 && a < 0.75) {
        double u = (a - 0.3) / 0.45, x = 86 + (10 - 86) * u, y = 46 + (28 - 46) * u - sin(u * PI_) * 14;
        line(tipX, tipY, x, y);
        bmp(FISH, rnd(x) - 3, rnd(y), false);
    } else {
        int bob = bite ? 2 + (int)((long long)floor(t * 8) % 2) : rnd(sin(t * 2));
        line(tipX, tipY, 87, 42 + bob);
        frect(86, 42 + bob, 3, 2); rect(86, 44 + bob, 3, 3);
        if (bite) { px(82, 46); px(92, 46); }
    }
    if (a >= 0.75 && a < 1) {
        px(5, 25 - (int)((long long)floor(t * 8) % 2));
        px(15, 25 - (int)((long long)floor(t * 8 + 1) % 2));
    }
}

// ---------- view N: Newton's cradle ----------
void drawCradle(double p, double t) {
    frect(30, 18, 69, 2); vline(32, 20, 61); vline(96, 20, 61); hline(26, 102, 62); hline(24, 104, 63);
    double A = 0.62 * fmax(0.15, 1 - 0.85 * clamp01(p)), s = sin(PI_ * t);
    const double L = 30;
    const int xs[5] = {48, 56, 64, 72, 80};
    for (int i = 0; i < 5; i++) {
        double th = 0;
        if (i == 4 && s > 0) th = A * s;
        if (i == 0 && s < 0) th = A * s;
        double bx = xs[i] + L * sin(th), by = 20 + L * cos(th);
        line(xs[i], 20, bx, by);
        circle(rnd(bx), rnd(by), 4);
        px(rnd(bx) - 1, rnd(by) - 2);
    }
    if (fabs(s) < 0.12) {
        int x = s >= 0 ? 85 : 43, d = s >= 0 ? 2 : -2;
        px(x, 46); px(x + d, 45); px(x + d, 51); px(x, 50);
    }
}

// ---------- view O: heartbeat trace ----------
int16_t ecg[W];
int ecgX = 0;
double ecgT = 0;
double ecgAccMs = 0;      // shown time not yet turned into trace columns
uint32_t ecgLastMs = 0;
bool ecgDrawnBefore = false;
const char *const HEART[] = {".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."};
const char *const HEART_S[] = {"##.##", "#####", ".###.", "..#.."};
double ecgWave(double ph) {
    if (ph > 0.1 && ph < 0.18) return -2 * sin((ph - 0.1) / 0.08 * PI_);
    if (ph >= 0.22 && ph < 0.25) return (ph - 0.22) / 0.03 * 2;
    if (ph >= 0.25 && ph < 0.27) return 2 - (ph - 0.25) / 0.02 * 18;
    if (ph >= 0.27 && ph < 0.30) return -16 + (ph - 0.27) / 0.03 * 21;
    if (ph >= 0.30 && ph < 0.32) return 5 - (ph - 0.30) / 0.02 * 5;
    if (ph > 0.42 && ph < 0.6) return -4 * sin((ph - 0.42) / 0.18 * PI_);
    return 0;
}
// The mockup writes 4 columns a frame at 10 frames a second (40 pixels a second, 25 ms of trace
// each). Here the trace advances by the real time it was shown: one column per 25 ms. A gap of more
// than 0.5 s (the view was hidden) counts as one frame, so the trace waits behind the count.
void drawEcg(double p, uint32_t nowMs) {
    double beat = 60 / (72 - 12 * clamp01(p));
    uint32_t dt = ecgDrawnBefore ? nowMs - ecgLastMs : 100;
    if (dt > 500) dt = 100;
    ecgLastMs = nowMs;
    ecgDrawnBefore = true;
    ecgAccMs += dt;
    while (ecgAccMs >= 25) {
        ecgAccMs -= 25;
        double v = 0;
        for (int k = 0; k < 8; k++) {
            ecgT += 0.025 / 8;
            double w = ecgWave(fmod(ecgT, beat) / beat);
            if (fabs(w) > fabs(v)) v = w;
        }
        ecg[ecgX] = (int16_t)rnd(44 + v);
        ecgX = (ecgX + 1) % W;
        for (int g = 0; g < 8; g++) ecg[(ecgX + g) % W] = -1;
    }
    for (int y = 24; y <= 60; y += 8) for (int x = 0; x < W; x += 8) px(x, y);
    for (int x = 1; x < W; x++) if (ecg[x] >= 0 && ecg[x - 1] >= 0) line(x - 1, ecg[x - 1], x, ecg[x]);
    int hx = (ecgX + W - 1) % W;
    if (ecg[hx] >= 0) fcircle(hx, ecg[hx], 1);
    double ph = fmod(ecgT, beat) / beat;
    if (ph > 0.24 && ph < 0.4) bmp(HEART, 113, 19, false);
    else bmp(HEART_S, 114, 20, false);
    bottomBar(p);
}

const char LETTERS[views::COUNT] = {'A', 'B', 'E', 'F', 'I', 'J', 'K', 'N', 'O'};
const char *const NAMES[views::COUNT] = {"hourglass", "plant", "coffee pot", "train", "scarf",
                                         "robot", "fishing", "Newton's cradle", "heartbeat"};

}  // namespace

namespace views {

void begin() {
    hourglassBegin();
    for (int i = 0; i < W; i++) ecg[i] = -1;
}

void render(uint8_t index, const Input &in, uint32_t nowMs) {
    uint32_t t0 = micros();
    memset(vb, 0, sizeof(vb));
    // Motion time from the chip's 64-bit microsecond clock, which does not wrap as millis() does
    // after 49.7 days.
    double t = time_us_64() / 1e6;
    long frame = frames++;
    switch (index) {
        case 0: drawHourglass(in.p, frame); break;
        case 1: drawPlant(in.p, t, in.breaks); break;
        case 2: drawCoffee(in.p, t); break;
        case 3: drawTrain(in.p, t, in.posMs, in.lenMs); break;
        case 4: drawScarf(in.p, t); break;
        case 5: drawRobot(in.p, t); break;
        case 6: drawFish(t, in.posMs); break;
        case 7: drawCradle(in.p, t); break;
        default: drawEcg(in.p, nowMs); break;
    }
    lastUs = micros() - t0;
    if (lastUs > maxUs) maxUs = lastUs;
}

const uint8_t *buffer() { return vb; }
char letter(uint8_t index) { return index < COUNT ? LETTERS[index] : '?'; }
const char *name(uint8_t index) { return index < COUNT ? NAMES[index] : "?"; }
uint32_t lastRenderUs() { return lastUs; }
uint32_t maxRenderUs() { return maxUs; }

}  // namespace views
