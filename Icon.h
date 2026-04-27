#pragma once
#include "raylib.h"
#include <string>
#include <algorithm>
#include <cctype>

struct Icon {
    std::string name;
    Rectangle   bounds;   // tile rect (label is drawn below the tile)
    Color       color;
};

static void DrawSingleIcon(const Icon& icon, bool hovered) {
    Color tile = icon.color;
    if (hovered) {
        tile.r = (unsigned char)std::min(255, tile.r + 55);
        tile.g = (unsigned char)std::min(255, tile.g + 55);
        tile.b = (unsigned char)std::min(255, tile.b + 55);
    }
    DrawRectangleRec(icon.bounds, tile);
    DrawRectangleLinesEx(icon.bounds,
                         hovered ? 2.0f : 1.0f,
                         hovered ? WHITE : Color{255, 255, 255, 75});

    // File extension inside tile — up to 4 chars, uppercase
    std::string ext;
    auto dot = icon.name.rfind('.');
    if (dot != std::string::npos) {
        ext = icon.name.substr(dot + 1);
        if (ext.size() > 4) ext = ext.substr(0, 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return (unsigned char)std::toupper(c); });
    }
    if (!ext.empty()) {
        const int fs = 11;
        const int tw = MeasureText(ext.c_str(), fs);
        DrawText(ext.c_str(),
                 (int)(icon.bounds.x + icon.bounds.width  / 2 - tw / 2),
                 (int)(icon.bounds.y + icon.bounds.height / 2 - fs / 2),
                 fs, WHITE);
    }

    // File name label below tile
    const int nfs = 12;
    const int ntw = MeasureText(icon.name.c_str(), nfs);
    const int lx  = (int)(icon.bounds.x + icon.bounds.width / 2 - ntw / 2);
    const int ly  = (int)(icon.bounds.y + icon.bounds.height + 5);
    DrawRectangle(lx - 3, ly - 1, ntw + 6, nfs + 2, Color{0, 0, 0, 110});
    DrawText(icon.name.c_str(), lx, ly, nfs,
             hovered ? WHITE : Color{215, 215, 215, 255});
}
