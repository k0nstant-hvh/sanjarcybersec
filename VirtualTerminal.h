#pragma once

// 1. Raylib first — types and function declarations are locked in.
#include "raylib.h"
#include "GuiWindow.h"
#include "GameManager.h"
#include "PacketGenerator.h"

// 2. Rename Windows symbols BEFORE including windows.h so their declarations
//    land under WIN_* names instead of colliding with Raylib's.
#define CloseWindow WIN_CloseWindow
#define ShowCursor  WIN_ShowCursor
#define Rectangle   WIN_Rectangle
#define DrawText    WIN_DrawText
#define DrawTextEx  WIN_DrawTextEx

#define NOGDI
#include <windows.h>
#include <mmsystem.h>   // PlaySoundA, MessageBeep — winmm.lib already linked

// 3. Remove the rename macros — all subsequent code uses Raylib's versions.
//    Windows equivalents remain accessible via the WIN_* aliases if needed.
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx

#include <chrono>
#include <deque>
#include <string>
#include <vector>
#include <algorithm>

// A GuiWindow whose content area is a playable terminal running the game engine.
// Double-clicking sanjar.exe on the desktop spawns one of these.
// Type 'start.exe' inside to activate the packet-defense game loop.
class VirtualTerminal : public GuiWindow {
public:
    VirtualTerminal(float x, float y, GameSettings* settings = nullptr);
    ~VirtualTerminal() override;

    void HandleInput() override;
    void UpdateGame(std::chrono::steady_clock::time_point now);

    bool HasGameJustEnded() { bool v = game_over_fired_; game_over_fired_ = false; return v; }

    int    GetFinalScore()     const { return gm_.score; }
    int    GetFinalWaves()     const { return gm_.current_wave - 1; }
    double GetSurviveSeconds() const { return survive_seconds_; }

protected:
    void DrawContent(bool focused) const override;

private:
    static constexpr int       MAX_LINES   = 200;
    static constexpr int       MAX_PKT_LOG = 12;
    static constexpr long long TICK_MS     = 50;    // 20 TPS cadence
    static constexpr float     FONT_SIZE   = 14.0f;
    static constexpr float     LINE_H      = FONT_SIZE + 2.0f;
    static constexpr float     INPUT_H     = 22.0f;
    static constexpr float     STATUS_H    = 22.0f;

    struct Line { std::string text; Color color; };

    std::vector<Line>                      lines_;
    std::string                            input_buf_;
    bool                                   game_active_      = false;
    bool                                   game_over_fired_  = false;
    bool                                   font_owned_       = false;
    Font                                   term_font_;

    GameManager                            gm_;
    PacketGenerator                        gen_;
    std::deque<PacketLogEntry>             pkt_log_;
    std::chrono::steady_clock::time_point  last_tick_;
    GameSettings*                          settings_         = nullptr;
    double                                 game_start_time_  = 0.0;
    double                                 survive_seconds_  = 0.0;

    void AddLine(const std::string& text, Color color = WHITE);
    void ProcessCommand(const std::string& cmd);
    void AddPacketLine(const Packet& pkt, int threat);
};

// ── Implementation (header-only, single translation unit) ─────────────────

inline VirtualTerminal::VirtualTerminal(float x, float y, GameSettings* settings)
    : GuiWindow("SANJAR Alpha Terminal", x, y, "", 640.0f, 420.0f)
    , last_tick_(std::chrono::steady_clock::now())
    , settings_(settings)
{
    // Attempt to load Consolas; fall back to Raylib default if not found.
    term_font_  = LoadFontEx("C:\\Windows\\Fonts\\consola.ttf",
                              (int)FONT_SIZE, nullptr, 0);
    font_owned_ = (term_font_.texture.id != GetFontDefault().texture.id);
    if (!font_owned_) term_font_ = GetFontDefault();

    AddLine("SANJAR v1.0  --  Cybersecurity Alpha Engine", {0, 200, 255, 255});
    AddLine("---------------------------------------------", {45, 50, 65, 255});
    AddLine("System initialized. All subsystems nominal.", {120, 200, 120, 255});
    AddLine("", WHITE);
    AddLine("  Type 'start.exe' to launch defense engine.", {175, 175, 175, 255});
    AddLine("  Type 'help' for a list of in-game commands.", {130, 130, 130, 255});
}

inline VirtualTerminal::~VirtualTerminal() {
    if (font_owned_) UnloadFont(term_font_);
}

inline void VirtualTerminal::AddLine(const std::string& text, Color color) {
    lines_.push_back({text, color});
    if ((int)lines_.size() > MAX_LINES)
        lines_.erase(lines_.begin());
}

// ── Keyboard input ─────────────────────────────────────────────────────────

inline void VirtualTerminal::HandleInput() {
    int ch;
    while ((ch = GetCharPressed()) != 0)
        if (ch >= 32 && ch <= 126 && (int)input_buf_.size() < 30)
            input_buf_ += static_cast<char>(ch);

    if (IsKeyPressed(KEY_BACKSPACE) && !input_buf_.empty())
        input_buf_.pop_back();

    if (IsKeyPressed(KEY_ENTER) && !input_buf_.empty()) {
        std::string cmd = input_buf_;
        input_buf_.clear();
        AddLine("> " + cmd, {0, 200, 80, 255});
        ProcessCommand(cmd);
    }
}

// ── Command routing ────────────────────────────────────────────────────────

inline void VirtualTerminal::ProcessCommand(const std::string& cmd) {
    if (!game_active_) {
        if (!gm_.is_alive()) {
            AddLine("  [OFFLINE] Close and reopen sanjar.exe to restart.",
                    {110, 110, 110, 255});
            return;
        }
        if (cmd == "start.exe") {
            game_active_     = true;
            game_start_time_ = GetTime();
            AddLine("[INIT] Launching packet defense engine...", {0, 200, 255, 255});
            AddLine("[INIT] Network traffic monitor active.", {0, 200, 255, 255});
            AddLine("---------------------------------------------", {45, 50, 65, 255});
            PlaySoundA("SystemStart", NULL, SND_ALIAS | SND_ASYNC);
        } else if (cmd == "help") {
            AddLine("  Commands: start.exe", {175, 175, 175, 255});
        } else {
            AddLine("  [ERR] Unknown: " + cmd, {200, 80, 80, 255});
            AddLine("  Type 'start.exe' to begin.", {110, 110, 110, 255});
        }
        return;
    }

    // ── In-game commands ──────────────────────────────────────────────────
    if (cmd == "accept" || cmd == "drop") {
        if (!gm_.active_packet) {
            AddLine("  [WARN] No active packet in queue.", {220, 180, 40, 255});
            return;
        }
        ActionResult res = gm_.on_action(cmd);
        if (res != ActionResult::UnknownCommand) {
            Color c = gm_.msg_positive ? Color{80, 210, 80, 255}
                                       : Color{210, 80, 80, 255};
            AddLine("  " + gm_.last_action_msg, c);
            auto it = std::find_if(pkt_log_.begin(), pkt_log_.end(),
                [](const PacketLogEntry& e){ return !e.handled && !e.leaked; });
            if (it != pkt_log_.end()) it->handled = true;
            if (res == ActionResult::AdminBlocked)   MessageBeep(MB_ICONEXCLAMATION);
            else if (res == ActionResult::AcceptedThreat) MessageBeep(MB_ICONHAND);
        }
    } else if (cmd == "quit") {
        gm_.on_action(cmd);
        AddLine("[SHUTDOWN] Defense engine offline.", {0, 200, 255, 255});
    } else if (cmd == "status") {
        AddLine("  Wave:" + std::to_string(gm_.current_wave)
                + "  Score:" + std::to_string(gm_.score)
                + "  HP:" + std::to_string(gm_.health) + "%",
                {200, 200, 100, 255});
    } else if (cmd == "help") {
        AddLine("  accept | drop | quit | status | help", {175, 175, 175, 255});
    } else if (cmd == "cmd_force_sql")   { gen_.force_next(ForceNext::SQL);
    } else if (cmd == "cmd_force_admin") { gen_.force_next(ForceNext::Admin);
    } else if (cmd == "cmd_force_junk")  { gen_.force_next(ForceNext::Junk);
    } else {
        AddLine("  [ERR] Unknown: '" + cmd + "'  (type 'help')", {200, 80, 80, 255});
    }
}

// ── Game tick (called every Raylib frame, self-throttled to 20 TPS) ────────

inline void VirtualTerminal::UpdateGame(std::chrono::steady_clock::time_point now) {
    if (!game_active_) return;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - last_tick_).count();
    if (ms < TICK_MS) return;
    last_tick_ = now;

    gm_.tick_message();
    if (settings_) {
        gen_.apply_settings(settings_->difficulty, settings_->spawn_rate);
        gm_.dev_mode = settings_->dev_mode;
    }

    if (gen_.ready(now)) {
        Packet pkt = gen_.generate();

        if (gm_.active_packet) {
            auto it = std::find_if(pkt_log_.begin(), pkt_log_.end(),
                [](const PacketLogEntry& e){ return !e.handled && !e.leaked; });
            if (it != pkt_log_.end()) it->leaked = true;

            AddLine("[LEAK] "
                    + gm_.active_packet->source_ip + ":"
                    + std::to_string(gm_.active_packet->port)
                    + " escaped!  [-15 HP]",
                    {210, 80, 80, 255});
        }

        int threat = gen_.heuristic_threat_score(pkt);
        gm_.arrive(pkt);
        pkt_log_.push_front({pkt, threat, now, false, false});
        if ((int)pkt_log_.size() > MAX_PKT_LOG) pkt_log_.pop_back();
        gen_.set_wave(gm_.current_wave);

        // Route packet data into the terminal lines vector
        AddPacketLine(pkt, threat);
    }

    if (!gm_.is_alive()) {
        game_active_     = false;
        game_over_fired_ = true;
        survive_seconds_ = GetTime() - game_start_time_;
        MessageBeep(MB_ICONSTOP);
        AddLine("=============================================", {210, 50, 50, 255});
        AddLine("  SYSTEM COMPROMISED  --  GAME OVER",         {210, 50, 50, 255});
        AddLine("  Final Score : " + std::to_string(gm_.score)
                + "  |  Waves : " + std::to_string(gm_.current_wave - 1),
                WHITE);
        AddLine("=============================================", {210, 50, 50, 255});
    }
}

inline void VirtualTerminal::AddPacketLine(const Packet& pkt, int threat) {
    std::string payload = pkt.payload;
    if ((int)payload.size() > 20) payload = payload.substr(0, 17) + "...";

    Color col = pkt.is_malicious ? Color{210, 80,  80, 255}
              : pkt.is_admin    ? Color{220, 180, 40, 255}
              :                   Color{80,  200, 80, 255};

    AddLine("[PKT] "
            + pkt.source_ip + ":" + std::to_string(pkt.port)
            + "  " + pkt.protocol
            + "  [" + std::to_string(threat) + "%]"
            + "  " + payload,
            col);
}

// ── Terminal rendering ─────────────────────────────────────────────────────

inline void VirtualTerminal::DrawContent(bool /*focused*/) const {
    Rectangle content = {
        bounds.x + 1.0f,
        (float)(bounds.y + WIN_TITLE_H),
        bounds.width  - 2.0f,
        bounds.height - WIN_TITLE_H - 1.0f
    };

    DrawRectangleRec(content, BLACK);
    BeginScissorMode((int)content.x, (int)content.y,
                     (int)content.width, (int)content.height);

    float text_x  = content.x + 5.0f;
    float lines_y = content.y + 4.0f;   // start of scrollback text

    // ── Status bar: shown while game is active or after game-over ─────────
    if (game_active_ || !gm_.is_alive()) {
        Color hp_col = gm_.health > 60 ? Color{80,  210,  80, 255}
                     : gm_.health > 30 ? Color{220, 180,  40, 255}
                     :                   Color{210,  80,  80, 255};

        std::string stat = "  Wave:" + std::to_string(gm_.current_wave)
                         + "  Score:" + std::to_string(gm_.score)
                         + "  HP:";
        std::string hp   = std::to_string(gm_.health) + "%";

        DrawTextEx(term_font_, stat.c_str(),
                   {text_x, content.y + 4.0f}, FONT_SIZE, 1.0f, RAYWHITE);
        float stat_w = MeasureTextEx(term_font_, stat.c_str(), FONT_SIZE, 1.0f).x;
        DrawTextEx(term_font_, hp.c_str(),
                   {text_x + stat_w, content.y + 4.0f}, FONT_SIZE, 1.0f, hp_col);

        DrawLine((int)content.x, (int)(content.y + STATUS_H),
                 (int)(content.x + content.width), (int)(content.y + STATUS_H),
                 {40, 45, 60, 255});
        lines_y = content.y + STATUS_H + 4.0f;
    }

    // ── Scrollback: show as many recent lines as fit above the input bar ──
    float lines_area_h = (content.y + content.height - INPUT_H) - lines_y;
    int   max_vis      = (int)(lines_area_h / LINE_H);
    int   start        = std::max(0, (int)lines_.size() - max_vis);

    for (int i = start; i < (int)lines_.size(); ++i) {
        DrawTextEx(term_font_, lines_[i].text.c_str(),
                   {text_x, lines_y + (float)(i - start) * LINE_H},
                   FONT_SIZE, 1.0f, lines_[i].color);
    }

    // ── Input bar ─────────────────────────────────────────────────────────
    float input_y = content.y + content.height - INPUT_H;
    DrawRectangle((int)content.x, (int)input_y,
                  (int)content.width, (int)INPUT_H, {6, 16, 6, 255});
    DrawLine((int)content.x, (int)input_y,
             (int)(content.x + content.width), (int)input_y, {0, 90, 25, 255});

    // Cursor blinks at 2 Hz
    bool        blink  = ((int)(GetTime() * 2.0) % 2) == 0;
    std::string prompt = "> " + input_buf_ + (blink ? "_" : " ");
    DrawTextEx(term_font_, prompt.c_str(),
               {text_x, input_y + 4.0f}, FONT_SIZE, 1.0f, {0, 220, 70, 255});

    EndScissorMode();
}
