#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include <algorithm>

static constexpr int WIN_TITLE_H  = 25;
static constexpr int WIN_CLOSE_W  = 30;
static constexpr int WIN_DEF_W    = 420;
static constexpr int WIN_DEF_H    = 280;
static constexpr int WIN_SCREEN_W = 1280;
static constexpr int WIN_SCREEN_H = 720;

enum class WinHit { None, Body, Close };

class GuiWindow {
public:
    std::string title;
    std::string bodyText; // Добавили поле для контента
    Rectangle   bounds;
    bool        open = true;

    // Обновили конструктор, чтобы он принимал текст (по умолчанию пустой)
    GuiWindow(const std::string& t, float x, float y, 
              const std::string& text = "", 
              float w = WIN_DEF_W, float h = WIN_DEF_H)
        : title(t), bodyText(text), bounds{x, y, w, h} {}

    virtual ~GuiWindow() = default;

    virtual void HandleInput() {}

    WinHit Update(Vector2 mouse, bool pressed, bool down) {
        if (!open) return WinHit::None;
        if (!down) dragging_ = false;

        if (dragging_) {
            bounds.x = mouse.x - drag_off_.x;
            bounds.y = mouse.y - drag_off_.y;
            bounds.x = std::max(0.0f, std::min(bounds.x, (float)WIN_SCREEN_W - bounds.width));
            bounds.y = std::max(0.0f, std::min(bounds.y, (float)WIN_SCREEN_H - (float)WIN_TITLE_H));
            return WinHit::Body;
        }

        if (!pressed || !CheckCollisionPointRec(mouse, bounds)) return WinHit::None;

        if (CheckCollisionPointRec(mouse, CloseRect())) {
            open = false;
            return WinHit::Close;
        }

        Rectangle tb = {bounds.x, bounds.y, bounds.width, (float)WIN_TITLE_H};
        if (CheckCollisionPointRec(mouse, tb)) {
            dragging_ = true;
            drag_off_ = {mouse.x - bounds.x, mouse.y - bounds.y};
        }
        return WinHit::Body;
    }

    void Draw(bool focused) const {
        DrawRectangle((int)bounds.x + 5, (int)bounds.y + 5,
                      (int)bounds.width, (int)bounds.height, {0, 0, 0, 65});
        DrawRectangleRec(bounds, {18, 20, 33, 245});
        DrawRectangleLinesEx(bounds, 1.0f,
            focused ? Color{70, 130, 220, 150} : Color{55, 60, 90, 95});

        DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, WIN_TITLE_H,
                      focused ? Color{28, 78, 170, 255} : Color{26, 30, 48, 255});
        DrawText(title.c_str(),
                 (int)bounds.x + 8,
                 (int)bounds.y + (WIN_TITLE_H - 14) / 2,
                 14, WHITE);

        DrawCloseButton();
        DrawContent(focused);
    }

protected:
    virtual void DrawContent(bool /*focused*/) const {
        // Теперь здесь рисуется реальный текст из bodyText
        DrawText(bodyText.c_str(),
                 (int)bounds.x + 14, (int)bounds.y + WIN_TITLE_H + 16,
                 15, {255, 255, 255, 200});
    }

private:
    bool    dragging_ = false;
    Vector2 drag_off_ = {};

    Rectangle CloseRect() const {
        return {bounds.x + bounds.width - WIN_CLOSE_W, bounds.y,
                (float)WIN_CLOSE_W, (float)WIN_TITLE_H};
    }

    void DrawCloseButton() const {
        Rectangle r   = CloseRect();
        bool      hov = CheckCollisionPointRec(GetMousePosition(), r);
        if (hov) DrawRectangleRec(r, {196, 43, 28, 255});
        const float cx = r.x + r.width  / 2.0f;
        const float cy = r.y + r.height / 2.0f;
        const float hs = 6.0f;
        DrawLineEx({cx - hs, cy - hs}, {cx + hs, cy + hs}, 1.5f, WHITE);
        DrawLineEx({cx + hs, cy - hs}, {cx - hs, cy + hs}, 1.5f, WHITE);
    }
};