#pragma once
#include "GuiWindow.h"
#include "GameManager.h"   // GameSettings
#include <cstdio>
#include <sstream>
#include <string>

class SettingsWindow : public GuiWindow {
public:
    SettingsWindow(float x, float y, GameSettings* settings)
        : GuiWindow("settings.json", x, y, "", 500.0f, 340.0f)
        , settings_(settings)
    {
        RebuildBuf();
    }

    void HandleInput() override {
        int ch;
        while ((ch = GetCharPressed()) != 0)
            if (ch >= 32 && ch <= 126 && (int)edit_buf_.size() < MAX_CHARS)
                edit_buf_ += (char)ch;

        if (IsKeyPressed(KEY_BACKSPACE) && !edit_buf_.empty())
            edit_buf_.pop_back();
        if (IsKeyPressed(KEY_ENTER) && (int)edit_buf_.size() < MAX_CHARS)
            edit_buf_ += '\n';

        bool ctrl_s    = IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S);
        bool btn_click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
                      && CheckCollisionPointRec(GetMousePosition(), SaveBtnRect());
        if (ctrl_s || btn_click) TrySave();
    }

protected:
    void DrawContent(bool focused) const override {
        const float CX = bounds.x + 8.0f;
        const float CY = bounds.y + WIN_TITLE_H + 8.0f;
        const float CW = bounds.width  - 16.0f;
        const float CH = bounds.height - WIN_TITLE_H - 50.0f;
        const float FS = 13.0f, LH = 16.0f;

        DrawRectangle((int)CX, (int)CY, (int)CW, (int)CH, {6, 9, 16, 255});
        DrawRectangleLinesEx({CX, CY, CW, CH}, 1.0f,
            focused ? Color{60, 120, 220, 200} : Color{40, 50, 80, 160});

        BeginScissorMode((int)CX + 2, (int)CY + 2, (int)CW - 4, (int)CH - 4);

        float ty     = CY + 6.0f;
        float last_x = CX + 6.0f;
        float last_y = ty;
        size_t start = 0;
        while (start <= edit_buf_.size()) {
            size_t nl      = edit_buf_.find('\n', start);
            bool   is_last = (nl == std::string::npos);
            std::string ln = edit_buf_.substr(start, is_last ? edit_buf_.size() - start : nl - start);
            if (ty + FS > CY + CH) break;
            DrawText(ln.c_str(), (int)(CX + 6), (int)ty, (int)FS, {160, 210, 255, 255});
            last_x = CX + 6.0f + MeasureText(ln.c_str(), (int)FS);
            last_y = ty;
            ty += LH;
            if (is_last) break;
            start = nl + 1;
        }
        // Blinking cursor
        if (focused && ((int)(GetTime() * 2)) % 2 == 0)
            DrawText("|", (int)last_x, (int)last_y, (int)FS, {160, 210, 255, 200});

        EndScissorMode();

        // Save button
        Rectangle btn = SaveBtnRect();
        bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
        DrawRectangleRec(btn, hov ? Color{50, 130, 230, 255} : Color{28, 82, 168, 255});
        const char* label = "Save  (Ctrl+S)";
        DrawText(label, (int)(btn.x + (btn.width - MeasureText(label, 12)) / 2),
                 (int)(btn.y + 7), 12, WHITE);

        // Status line
        if (!status_msg_.empty()) {
            Color sc = status_ok_ ? Color{80, 210, 80, 255} : Color{220, 60, 60, 255};
            DrawText(status_msg_.c_str(),
                     (int)(bounds.x + 10),
                     (int)(bounds.y + bounds.height - 26), 12, sc);
        }
    }

private:
    static constexpr int MAX_CHARS = 600;

    GameSettings* settings_;
    std::string   edit_buf_;
    std::string   status_msg_;
    bool          status_ok_ = true;

    Rectangle SaveBtnRect() const {
        return {bounds.x + bounds.width - 140.0f,
                bounds.y + bounds.height - 38.0f,
                126.0f, 26.0f};
    }

    void RebuildBuf() {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\n"
                 "  \"difficulty\": %.2f,\n"
                 "  \"spawn_rate\": %.2f,\n"
                 "  \"theme\": \"Win11_Dark\",\n"
                 "  \"admin\": [\"10.0.0.50\"],\n"
                 "  \"dev_mode\": %s\n"
                 "}",
                 settings_ ? settings_->difficulty : 1.5f,
                 settings_ ? settings_->spawn_rate  : 0.8f,
                 (settings_ && settings_->dev_mode) ? "true" : "false");
        edit_buf_ = buf;
    }

    void TrySave() {
        if (edit_buf_.empty()) {
            status_msg_ = "Save Failed: file is empty.";
            status_ok_  = false;
            return;
        }
        float d = 1.5f, s = 0.8f;
        bool  dm = false;
        if (!ParseJson(edit_buf_, d, s, dm)) {
            status_msg_ = "Save Failed: invalid JSON.";
            status_ok_  = false;
            return;
        }
        if (settings_) {
            settings_->difficulty = d;
            settings_->spawn_rate = s;
            settings_->dev_mode   = dm;
        }
        status_msg_ = "Saved.";
        status_ok_  = true;
    }

    static bool ParseJson(const std::string& json,
                          float& difficulty, float& spawn_rate, bool& dev_mode) {
        size_t first = json.find_first_not_of(" \t\n\r");
        size_t last  = json.find_last_not_of(" \t\n\r");
        if (first == std::string::npos || json[first] != '{' || json[last] != '}')
            return false;

        auto readFloat = [&](const char* key, float& out) -> bool {
            std::string k = std::string("\"") + key + "\"";
            auto pos = json.find(k);
            if (pos == std::string::npos) return false;
            pos = json.find(':', pos + k.size());
            if (pos == std::string::npos) return false;
            return sscanf(json.c_str() + pos + 1, " %f", &out) == 1;
        };

        auto readBool = [&](const char* key, bool& out) -> bool {
            std::string k = std::string("\"") + key + "\"";
            auto pos = json.find(k);
            if (pos == std::string::npos) return false;
            pos = json.find(':', pos + k.size());
            if (pos == std::string::npos) return false;
            size_t vp = json.find_first_not_of(" \t\n\r", pos + 1);
            if (vp == std::string::npos) return false;
            if (json.compare(vp, 4, "true")  == 0) { out = true;  return true; }
            if (json.compare(vp, 5, "false") == 0) { out = false; return true; }
            return false;
        };

        return readFloat("difficulty", difficulty)
            && readFloat("spawn_rate",  spawn_rate)
            && readBool ("dev_mode",    dev_mode);
    }
};
