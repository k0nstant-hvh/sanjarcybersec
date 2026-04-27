#include "raylib.h"
#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "Icon.h"
#include "GuiWindow.h"
#include "VirtualTerminal.h"   // brings in GameManager, PacketGenerator, Packet

static constexpr int SCREEN_W  = 1280;
static constexpr int SCREEN_H  = 720;
static constexpr int TASKBAR_H = 40;

// Добавь это в начало main() или перед основным циклом while(!WindowShouldClose())
std::string guideContent = "S.A.N.J.A.R. SYSTEM DEFENSE GUIDE v1.0\n"
                           "--------------------------------------\n"
                           "1. SQL INJECTION (SQLi):\n"
                           "   Look for: 'OR 1=1', 'SELECT *', 'DROP TABLE'.\n"
                           "   Action: IMMEDIATELY DROP.\n\n"
                           "2. REMOTE CODE EXECUTION (RCE):\n"
                           "   Look for: 'base64_decode', 'eval()', 'exec()'.\n"
                           "   Action: HIGH THREAT.\n\n"
                           "3. FRIENDLY TRAFFIC:\n"
                           "   Admin IP: 10.0.0.50\n"
                           "   Action: ALWAYS ACCEPT (50% HP Penalty if blocked).\n\n"
                           "COMMANDS:\n"
                           "- 'start.exe' to begin.\n"
                           "- 'clear' to wipe history.";

std::string settingsContent = "{\n"
                              "  \"difficulty\": 1.5,\n"
                              "  \"spawn_rate\": 0.8,\n"
                              "  \"theme\": \"Win11_Dark\",\n"
                              "  \"admin\": [\"10.0.0.50\"],\n"
                              "  \"dev_mode\": false\n"
                              "}";

enum class GameState { DESKTOP, MODAL_OPEN };
 // ── Desktop icons ─────────────────────────────────────────────────────
    const std::vector<Icon> icons = {
        {"sanjar.exe",    {50,  55, 64, 64}, { 40, 100, 210, 255}},
        {"guide.txt",     {50, 185, 64, 64}, { 50, 160,  75, 255}},
        {"settings.json", {50, 315, 64, 64}, {200, 110,  35, 255}},
    };
    
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

// ── Entry point ────────────────────────────────────────────────────────────
int main() {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "SANJAR — Cybersecurity Defense System");
    SetTargetFPS(60);

    GameState state = GameState::DESKTOP;

    // ── Desktop icons ─────────────────────────────────────────────────────
    const std::vector<Icon> icons = {
        {"sanjar.exe",    {50,  55, 64, 64}, { 40, 100, 210, 255}},
        {"guide.txt",     {50, 185, 64, 64}, { 50, 160,  75, 255}},
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

        // ── Tick all VirtualTerminals regardless of focus ─────────────────
        for (auto& w : windows)
            if (auto* term = dynamic_cast<VirtualTerminal*>(w.get()))
                term->UpdateGame(frame_now);

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

        // ── Icon double-click: spawn window ───────────────────────────────
if (!click_captured && pressed) {
    for (int i = 0; i < (int)icons.size(); ++i) {
        if (CheckCollisionPointRec(mouse, icons[i].bounds)) {
            double t = GetTime();
            if (t - icon_last_click[i] < 0.35) {
                float off = (float)(windows.size() % 8) * 28.0f;
                float wx  = 200.0f + off, wy = 100.0f + off;

                if (i == 0) { // sanjar.exe
                    windows.push_back(std::make_unique<VirtualTerminal>(wx, wy));
                } 
                else if (i == 1) { // guide.txt
                    std::string guideTxt = "S.A.N.J.A.R. DEFENSE GUIDE\n\n1. SQLi: Watch for 'OR 1=1'\n2. RCE: Watch for 'eval()'\n3. ADMIN: 10.0.0.50 (ACCEPT ONLY)\n\nType 'start.exe' in terminal.";
                    windows.push_back(std::make_unique<GuiWindow>(icons[i].name, wx, wy, guideTxt));
                } 
                else if (i == 2) { // settings.json
                    std::string settingsJson = "{\n  \"difficulty\": 1.5,\n  \"spawn_rate\": 0.8,\n  \"theme\": \"Win11_Dark\",\n  \"dev_mode\": false\n}";
                    windows.push_back(std::make_unique<GuiWindow>(icons[i].name, wx, wy, settingsJson));
                }
                
                icon_last_click[i] = -1.0;
            } else {
                icon_last_click[i] = t;
            }
            break;
        }
    }
}

        // ── Keyboard: route only to the focused (topmost) window ──────────
        if (!windows.empty())
            windows.back()->HandleInput();

        // ── Render ────────────────────────────────────────────────────────
        BeginDrawing();
        DrawDesktop(state, icons, windows, mouse);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
