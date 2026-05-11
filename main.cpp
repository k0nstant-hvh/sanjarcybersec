#include "raylib.h"
#include <chrono>
#include <cmath>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "Icon.h"
#include "GuiWindow.h"
#include "VirtualTerminal.h"   // brings in GameManager, PacketGenerator, Packet
#include "SettingsWindow.h"
#include "GuideWindow.h"

static constexpr int SCREEN_W  = 1280;
static constexpr int SCREEN_H  = 720;
static constexpr int TASKBAR_H = 40;


enum class GameState { DESKTOP, MODAL_OPEN, CRASH_WARNING, SHUTDOWN_ANIM, BOOTLOOP };

// ── Wallpaper ──────────────────────────────────────────────────────────────
static void DrawWallpaper() {
    ClearBackground({5, 8, 22, 255});
    const int cx = SCREEN_W / 2;
    const int cy = (SCREEN_H - TASKBAR_H) / 2;
    struct Layer { float rx, ry; unsigned char r, g, b, a; };
    static constexpr Layer layers[] = {
        {640, 460,  15,  35, 120, 12},
        {520, 374,  20,  55, 160, 20},
        {400, 288,  35,  85, 200, 30},
        {280, 202,  55, 120, 225, 45},
        {160, 115,  85, 155, 245, 65},
        { 80,  58, 130, 190, 255, 90},
    };
    for (const auto& l : layers)
        DrawEllipse(cx, cy, l.rx, l.ry, {l.r, l.g, l.b, l.a});
}

// ── Taskbar ────────────────────────────────────────────────────────────────
static void DrawTaskbar() {
    const int ty = SCREEN_H - TASKBAR_H;
    DrawRectangle(0, ty, SCREEN_W, TASKBAR_H, {12, 12, 18, 200});
    DrawLine(0, ty, SCREEN_W, ty, {255, 255, 255, 20});

    time_t     now = time(nullptr);
    struct tm* lt  = localtime(&now);
    char       buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", lt);
    const int fs = 18;
    const int tw = MeasureText(buf, fs);
    DrawText(buf, SCREEN_W - tw - 16, ty + (TASKBAR_H - fs) / 2, fs, WHITE);
}

// ── Helpers for unique_ptr window vector ───────────────────────────────────
using WinVec = std::vector<std::unique_ptr<GuiWindow>>;

static bool AnyWindowHits(const WinVec& ws, Vector2 p) {
    for (const auto& w : ws)
        if (w->open && CheckCollisionPointRec(p, w->bounds)) return true;
    return false;
}

// ── Desktop composite render ───────────────────────────────────────────────
// Layer order: wallpaper → icons → windows (back-to-front) → taskbar (always top).
static void DrawDesktop(GameState /*state*/,
                         const std::vector<Icon>& icons,
                         const WinVec&            windows,
                         Vector2                  mouse) {
    DrawWallpaper();

    for (const auto& icon : icons) {
        bool hov = !AnyWindowHits(windows, mouse)
                && CheckCollisionPointRec(mouse, icon.bounds);
        DrawSingleIcon(icon, hov);
    }

    // Index 0 = bottom-most, last index = focused (on top).
    for (int i = 0; i < (int)windows.size(); ++i)
        windows[i]->Draw(i == (int)windows.size() - 1);

    DrawTaskbar();
}

// ── Crash-sequence helpers ─────────────────────────────────────────────────

static void DrawCrashWarning(double enter_time) {
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, {60, 0, 0, 100});

    const int mw = 520, mh = 170;
    const int mx = (SCREEN_W - mw) / 2;
    const int my = (SCREEN_H - mh) / 2;
    DrawRectangle(mx, my, mw, mh, {14, 8, 8, 250});
    DrawRectangleLinesEx({(float)mx, (float)my, (float)mw, (float)mh}, 2.0f, {210, 40, 40, 255});

    int secs = 5 - (int)(GetTime() - enter_time);
    if (secs < 1) secs = 1;

    const char* title = "! SYSTEM ALERT !";
    DrawText(title, mx + (mw - MeasureText(title, 22)) / 2, my + 22, 22, {220, 55, 55, 255});

    char body[96];
    snprintf(body, sizeof(body),
             "Your session will be terminated in %d second%s...",
             secs, secs == 1 ? "" : "s");
    DrawText(body, mx + (mw - MeasureText(body, 16)) / 2, my + 72, 16, WHITE);

    if (((int)(GetTime() * 2)) % 2 == 0) {
        const char* note = "Backup your data now.";
        DrawText(note, mx + (mw - MeasureText(note, 13)) / 2, my + 115, 13, {200, 200, 80, 255});
    }
}

static void DrawShutdownAnim(double enter_time) {
    float t = (float)(GetTime() - enter_time);

    const char* msg = "Shutting down...";
    DrawText(msg, (SCREEN_W - MeasureText(msg, 28)) / 2, SCREEN_H / 2 - 70, 28, {200, 200, 200, 255});

    float    angle  = fmodf(t * 220.0f, 360.0f);
    Vector2  center = {SCREEN_W / 2.0f, SCREEN_H / 2.0f + 20.0f};
    DrawRing(center, 26.0f, 38.0f, angle, angle + 270.0f, 32, {80, 140, 255, 220});
}

static void DrawBiosLogo(int idx, int cx, int cy, bool glitch) {
    auto rnd = [](int lo, int hi) -> unsigned char { return (unsigned char)GetRandomValue(lo, hi); };
    Color accent  = glitch ? Color{rnd(150,255), rnd(0,50),  rnd(0,50),  255} : Color{210, 30, 30, 255};
    Color white_c = glitch ? Color{rnd(180,255), rnd(180,255), rnd(180,255), 255} : WHITE;

    switch (idx) {
    case 0: { // ASUS ROG
        const char* sub  = "REPUBLIC OF GAMERS";
        const char* name = "ROG";
        DrawText(sub,  cx - MeasureText(sub,  18) / 2, cy - 46, 18, {150, 150, 150, 255});
        DrawText(name, cx - MeasureText(name, 54) / 2, cy - 14, 54, accent);
        DrawEllipseLines(cx, cy + 56, 34, 13, accent);
        DrawCircle(cx, cy + 56, 8, accent);
        break;
    }
    case 1: { // MSI
        const char* name = "MSI";
        const char* sub  = "GAMING";
        DrawText(name, cx - MeasureText(name, 64) / 2, cy - 32, 64, accent);
        DrawText(sub,  cx - MeasureText(sub,  22) / 2, cy + 44, 22, {160, 160, 160, 255});
        break;
    }
    case 2: { // AORUS
        Color gold = glitch ? accent : Color{220, 175, 35, 255};
        const char* name = "AORUS";
        const char* sub  = "Powered by GIGABYTE";
        DrawText(name, cx - MeasureText(name, 54) / 2, cy - 27, 54, gold);
        DrawText(sub,  cx - MeasureText(sub,  16) / 2, cy + 40, 16, {120, 120, 120, 255});
        break;
    }
    default: { // AMD Ryzen
        const char* name  = "AMD";
        const char* brand = "Ryzen";
        const char* line  = "SERIES";
        DrawText(name,  cx - MeasureText(name,  58) / 2, cy - 36, 58, accent);
        DrawText(brand, cx - MeasureText(brand, 32) / 2, cy + 30, 32, white_c);
        DrawText(line,  cx - MeasureText(line,  18) / 2, cy + 68, 18, {120, 120, 120, 255});
        break;
    }
    }
}

static void DrawWindowsSpinner(int cx, int cy, float elapsed, bool glitch) {
    static constexpr int   N      = 5;
    static constexpr float RADIUS = 20.0f;
    static constexpr float PI_F   = 3.14159265f;
    float base_deg = fmodf(elapsed * 240.0f, 360.0f);

    for (int i = 0; i < N; ++i) {
        float ang = (base_deg + i * (360.0f / N)) * (PI_F / 180.0f);
        float x   = cx + RADIUS * cosf(ang);
        float y   = cy + RADIUS * sinf(ang);
        unsigned char a   = (unsigned char)(255 - i * 45);
        float         dot = 4.5f - i * 0.4f;
        Color c = glitch
            ? Color{(unsigned char)GetRandomValue(180,255), 80, 80, a}
            : Color{220, 220, 220, a};
        DrawCircle((int)x, (int)y, dot, c);
    }
}

// ── Boot-phase helpers ─────────────────────────────────────────────────────

static Rectangle GetRecoveryOptionRect(int i) {
    return {80.0f + i * 240.0f, 200.0f, 220.0f, 90.0f};
}

static Rectangle GetOKBtnRect() {
    return {(float)(SCREEN_W / 2 - 50), (float)(SCREEN_H / 2 + 55), 100.0f, 30.0f};
}

static void DrawBootPhaseA(int logo_idx, double enter_time) {
    float elapsed = (float)(GetTime() - enter_time);
    bool  glitch  = elapsed < 0.18f;
    int ox = glitch ? GetRandomValue(-8, 8) : 0;
    int oy = glitch ? GetRandomValue(-4, 4) : 0;
    if (glitch) {
        for (int i = 0; i < 4; ++i)
            DrawRectangle(GetRandomValue(-5, 0), GetRandomValue(0, SCREEN_H),
                SCREEN_W + 10, GetRandomValue(1, 4),
                {(unsigned char)GetRandomValue(150, 255),
                 (unsigned char)GetRandomValue(0, 60),
                 (unsigned char)GetRandomValue(0, 60),
                 (unsigned char)GetRandomValue(60, 140)});
    }
    DrawBiosLogo(logo_idx, SCREEN_W / 2 + ox, SCREEN_H / 2 + oy - 40, glitch);
    const char* v    = "UEFI BIOS v3.14.1  |  64MB ROM";
    const char* hint = "Press DEL to enter Setup  |  F11: Boot Menu";
    DrawText(v,    (SCREEN_W - MeasureText(v,    11)) / 2, SCREEN_H - 80, 11, {50, 50, 50, 255});
    DrawText(hint, (SCREEN_W - MeasureText(hint, 11)) / 2, SCREEN_H - 60, 11, {50, 50, 50, 255});
}

static void DrawBootPhaseB(double enter_time) {
    float elapsed = (float)(GetTime() - enter_time);
    const int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
    const int SQ = 38, GAP = 6;
    DrawRectangle(cx - SQ - GAP, cy - SQ - GAP, SQ, SQ, {242,  80,  34, 255});
    DrawRectangle(cx + GAP,      cy - SQ - GAP, SQ, SQ, {127, 186,   0, 255});
    DrawRectangle(cx - SQ - GAP, cy + GAP,      SQ, SQ, {  0, 161, 241, 255});
    DrawRectangle(cx + GAP,      cy + GAP,      SQ, SQ, {255, 185,   0, 255});
    const char* wintxt = "Windows";
    DrawText(wintxt, cx - MeasureText(wintxt, 28) / 2, cy + SQ + GAP + 24, 28, WHITE);
    DrawWindowsSpinner(cx, cy + SQ + GAP + 80, elapsed, false);
}

static void DrawBootPhaseC(double enter_time, Vector2 mouse) {
    double elapsed = GetTime() - enter_time;
    DrawText("Recovery", 80, 60, 52, WHITE);
    DrawText("It looks like Windows didn't load correctly.",
             80, 128, 16, {190, 215, 255, 255});

    static const char* titles[] = {"System Restore",   "Reset this PC",     "Continue"};
    static const char* desc1[]  = {"Undo recent system","Remove all apps and","Exit and continue"};
    static const char* desc2[]  = {"changes.",          "reinstall Windows.", "to Windows."};
    for (int i = 0; i < 3; ++i) {
        Rectangle r = GetRecoveryOptionRect(i);
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? Color{30, 100, 200, 255} : Color{20, 70, 150, 200});
        DrawRectangleLinesEx(r, 1.5f, {80, 140, 230, 200});
        DrawText(titles[i], (int)(r.x + 12), (int)(r.y + 12), 15, WHITE);
        DrawText(desc1[i],  (int)(r.x + 12), (int)(r.y + 44), 12, {175, 205, 240, 255});
        DrawText(desc2[i],  (int)(r.x + 12), (int)(r.y + 59), 12, {175, 205, 240, 255});
    }

    int secs = 4 - (int)elapsed;
    if (secs >= 0) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Continuing in %d...", secs + 1);
        DrawText(buf, SCREEN_W - MeasureText(buf, 12) - 16, SCREEN_H - 38, 12,
                 {130, 160, 210, 200});
    }
}

static void DrawBootPhaseD(double enter_time, Vector2 mouse) {
    double elapsed   = GetTime() - enter_time;
    int    secs_left = std::max(0, 5 - (int)elapsed);

    const int MW = 460, MH = 220;
    const int MX = (SCREEN_W - MW) / 2, MY = (SCREEN_H - MH) / 2;
    DrawRectangle(MX, MY, MW, MH, {22, 24, 38, 252});
    DrawRectangleLinesEx({(float)MX, (float)MY, (float)MW, (float)MH}, 2.0f,
                         {70, 110, 200, 255});
    DrawRectangle(MX, MY, MW, 36, {40, 60, 150, 255});
    DrawText("  Critical System Error", MX + 8, MY + 10, 16, WHITE);
    DrawText("A critical system error has been detected.",   MX + 20, MY + 52, 14, WHITE);
    DrawText("System integrity cannot be guaranteed.",       MX + 20, MY + 72, 13,
             {170, 185, 215, 255});
    DrawText("All active sessions will be terminated.",      MX + 20, MY + 92, 13,
             {170, 185, 215, 255});

    char buf[64];
    snprintf(buf, sizeof(buf), "Auto-closing in %d second%s...",
             secs_left, secs_left == 1 ? "" : "s");
    DrawText(buf, MX + 20, MY + 125, 12, {120, 140, 190, 255});

    Rectangle btn = GetOKBtnRect();
    bool hov = CheckCollisionPointRec(mouse, btn);
    DrawRectangleRec(btn, hov ? Color{70, 120, 230, 255} : Color{45, 80, 175, 255});
    DrawRectangleLinesEx(btn, 1.0f, {100, 140, 240, 200});
    const char* ok = "OK";
    DrawText(ok, (int)(btn.x + (btn.width - MeasureText(ok, 14)) / 2),
             (int)(btn.y + 8), 14, WHITE);
}

static void DrawBootPhaseGlitch(double enter_time) {
    float progress = std::min(1.0f, (float)(GetTime() - enter_time) / 2.0f);
    int   num      = (int)(progress * 250);
    for (int i = 0; i < num; ++i) {
        int sz = GetRandomValue(8, 90);
        DrawRectangle(GetRandomValue(0, SCREEN_W), GetRandomValue(0, SCREEN_H), sz, sz,
            {(unsigned char)GetRandomValue(0, 255),
             (unsigned char)GetRandomValue(0, 255),
             (unsigned char)GetRandomValue(0, 255),
             (unsigned char)GetRandomValue(80, 255)});
    }
}

// ── Post-game report builder ───────────────────────────────────────────────
static std::string BuildGameReport(VirtualTerminal* t, const GameSettings& s) {
    double sec = t->GetSurviveSeconds();
    int m = (int)(sec / 60.0), ss = (int)sec % 60;
    char buf[512];
    snprintf(buf, sizeof(buf),
             "== LAST GAME REPORT ==\n\n"
             "Final Score    : %d\n"
             "Waves Survived : %d\n"
             "Surviving Time : %dm %ds\n\n"
             "Settings Used:\n"
             "  difficulty   : %.2f\n"
             "  spawn_rate   : %.2f\n"
             "  dev_mode     : %s",
             t->GetFinalScore(), t->GetFinalWaves(), m, ss,
             s.difficulty, s.spawn_rate, s.dev_mode ? "true" : "false");
    return buf;
}

// ── Loading cursor (Windows-style blue spinning arc) ──────────────────────
static void DrawLoadingCursor(Vector2 pos) {
    float angle = fmodf((float)GetTime() * 300.0f, 360.0f);
    DrawRing(pos, 8.0f, 14.0f, angle,          angle + 250.0f, 20, {30, 120, 215, 230});
    DrawRing(pos, 8.0f, 14.0f, angle + 250.0f, angle + 360.0f, 10, {30, 120, 215,  55});
}

// ── Entry point ────────────────────────────────────────────────────────────
int main() {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "SANJAR — Cybersecurity Defense System");
    SetTargetFPS(60);

    GameState    state            = GameState::DESKTOP;
    double       state_enter_time = 0.0;
    int          bootlogo_idx     = GetRandomValue(0, 3);
    int          boot_phase       = 0;
    double       phase_enter_time = 0.0;
    GameSettings settings;
    std::string  report_text;
    bool         report_icon_added = false;

    // ── Desktop icons ─────────────────────────────────────────────────────
    std::vector<Icon> icons = {
        {"sanjar.exe",    {50,  55, 64, 64}, { 40, 100, 210, 255}},
        {"guide.pdf",     {50, 185, 64, 64}, { 50, 160,  75, 255}},
        {"settings.json", {50, 315, 64, 64}, {200, 110,  35, 255}},
    };

    // ── Window stack & double-click state ─────────────────────────────────
    WinVec              windows;
    std::vector<double> icon_last_click(icons.size(), -1.0);

    while (!WindowShouldClose()) {
        auto    frame_now = std::chrono::steady_clock::now();
        Vector2 mouse     = GetMousePosition();
        bool    pressed   = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool    down      = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        if (state == GameState::DESKTOP) {
            // ── Tick all VirtualTerminals ─────────────────────────────────
            for (auto& w : windows) {
                VirtualTerminal* term = dynamic_cast<VirtualTerminal*>(w.get());
                if (term) term->UpdateGame(frame_now);
            }

            // ── Check for game-over → build report, add icon, crash sequence ─
            for (auto& w : windows) {
                VirtualTerminal* t = dynamic_cast<VirtualTerminal*>(w.get());
                if (t && t->HasGameJustEnded()) {
                    report_text = BuildGameReport(t, settings);
                    if (!report_icon_added) {
                        icons.push_back({"last_game_report.txt",
                            {50.0f, 445.0f, 64.0f, 64.0f}, {80, 80, 180, 255}});
                        icon_last_click.push_back(-1.0);
                        report_icon_added = true;
                    }
                    state            = GameState::CRASH_WARNING;
                    state_enter_time = GetTime();
                    break;
                }
            }

            // ── Window update: iterate top-to-bottom; first hit captures click ─
            bool click_captured = false;
            for (int i = (int)windows.size() - 1; i >= 0; --i) {
                WinHit hit = windows[i]->Update(mouse, pressed && !click_captured, down);

                if (hit == WinHit::Close) {
                    windows.erase(windows.begin() + i);
                    click_captured = true;
                    break;
                }
                if (hit == WinHit::Body) {
                    // Promote clicked window to top (last = focused = rendered last).
                    if (i != (int)windows.size() - 1) {
                        auto w = std::move(windows[i]);
                        windows.erase(windows.begin() + i);
                        windows.push_back(std::move(w));
                    }
                    click_captured = true;
                    break;
                }
            }

            // ── Icon drag: update position while held, clear when released ─
            if (!down) {
                for (auto& ic : icons) ic.dragging = false;
            } else {
                for (auto& ic : icons) {
                    if (ic.dragging) {
                        ic.bounds.x = std::max(0.0f, std::min(mouse.x - ic.drag_off.x,
                                                               (float)SCREEN_W - ic.bounds.width));
                        ic.bounds.y = std::max(0.0f, std::min(mouse.y - ic.drag_off.y,
                                                               (float)(SCREEN_H - TASKBAR_H) - ic.bounds.height));
                    }
                }
            }

            // ── Icon press: start drag + double-click → spawn with busy delay ─
            if (!click_captured && pressed) {
                for (int i = 0; i < (int)icons.size(); ++i) {
                    if (CheckCollisionPointRec(mouse, icons[i].bounds)) {
                        // Begin drag from this press.
                        icons[i].dragging = true;
                        icons[i].drag_off = {mouse.x - icons[i].bounds.x,
                                             mouse.y - icons[i].bounds.y};

                        double t = GetTime();
                        if (t - icon_last_click[i] < 0.35) {
                            float  off   = (float)(windows.size() % 8) * 28.0f;
                            float  wx    = 200.0f + off, wy = 100.0f + off;
                            double delay = 0.5 + GetRandomValue(0, 150) / 100.0;

                            std::unique_ptr<GuiWindow> win;
                            const std::string& nm = icons[i].name;
                            if (nm == "sanjar.exe") {
                                win = std::make_unique<VirtualTerminal>(wx, wy, &settings);
                            } else if (nm == "guide.pdf") {
                                win = std::make_unique<GuideWindow>(wx, wy);
                            } else if (nm == "settings.json") {
                                win = std::make_unique<SettingsWindow>(wx, wy, &settings);
                            } else if (nm == "last_game_report.txt") {
                                win = std::make_unique<GuiWindow>(nm, wx, wy, report_text, 480.0f, 300.0f);
                            }
                            if (win) {
                                win->load_finish_time = GetTime() + delay;
                                windows.push_back(std::move(win));
                            }

                            icon_last_click[i] = -1.0;
                        } else {
                            icon_last_click[i] = t;
                        }
                        break;
                    }
                }
            }

            // ── Keyboard: route only to the focused (topmost) window ──────
            if (!windows.empty())
                windows.back()->HandleInput();
        }

        // ── Crash sequence state transitions ─────────────────────────────
        if (state == GameState::CRASH_WARNING && GetTime() - state_enter_time >= 5.0) {
            state            = GameState::SHUTDOWN_ANIM;
            state_enter_time = GetTime();
        }
        if (state == GameState::SHUTDOWN_ANIM && GetTime() - state_enter_time >= 3.0) {
            state            = GameState::BOOTLOOP;
            state_enter_time = GetTime();
            boot_phase       = 0;
            phase_enter_time = GetTime();
        }
        if (state == GameState::BOOTLOOP) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                state      = GameState::DESKTOP;
                boot_phase = 0;
            } else {
                double pe = GetTime() - phase_enter_time;
                if (boot_phase == 0 && pe >= 2.0) {
                    boot_phase = 1; phase_enter_time = GetTime();
                } else if (boot_phase == 1 && pe >= 7.0) {
                    boot_phase = 2; phase_enter_time = GetTime();
                } else if (boot_phase == 2) {
                    bool tile_click = false;
                    if (pressed && pe >= 0.3) {
                        for (int i = 0; i < 3; ++i)
                            if (CheckCollisionPointRec(mouse, GetRecoveryOptionRect(i)))
                                { tile_click = true; break; }
                    }
                    if (tile_click || pe >= 4.0) {
                        boot_phase = 3; phase_enter_time = GetTime();
                    }
                } else if (boot_phase == 3) {
                    bool ok_click = pressed && CheckCollisionPointRec(mouse, GetOKBtnRect());
                    if (ok_click || pe >= 5.0) {
                        boot_phase = 4; phase_enter_time = GetTime();
                    }
                } else if (boot_phase == 4 && pe >= 2.0) {
                    boot_phase       = 0;
                    phase_enter_time = GetTime();
                }
            }
        }

        // ── Cursor: hide system cursor and draw spinner when any window loads ─
        bool any_loading = false;
        if (state == GameState::DESKTOP) {
            for (const auto& w : windows)
                if (w->is_loading()) { any_loading = true; break; }
        }
        if (any_loading) HideCursor(); else ShowCursor();

        // ── Render ────────────────────────────────────────────────────────
        BeginDrawing();
        switch (state) {
        case GameState::DESKTOP:
        case GameState::MODAL_OPEN:
            DrawDesktop(state, icons, windows, mouse);
            break;
        case GameState::CRASH_WARNING:
            DrawDesktop(state, icons, windows, mouse);
            DrawCrashWarning(state_enter_time);
            break;
        case GameState::SHUTDOWN_ANIM:
            ClearBackground(BLACK);
            DrawShutdownAnim(state_enter_time);
            break;
        case GameState::BOOTLOOP:
            ClearBackground(boot_phase >= 2 ? Color{0, 120, 215, 255} : BLACK);
            if      (boot_phase == 0) DrawBootPhaseA(bootlogo_idx, phase_enter_time);
            else if (boot_phase == 1) DrawBootPhaseB(phase_enter_time);
            else if (boot_phase == 2) DrawBootPhaseC(phase_enter_time, mouse);
            else if (boot_phase == 3) DrawBootPhaseD(phase_enter_time, mouse);
            else if (boot_phase == 4) DrawBootPhaseGlitch(phase_enter_time);
            break;
        }
        if (any_loading) DrawLoadingCursor(mouse);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
