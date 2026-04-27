# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**SANJAR** is a console-based cybersecurity defense game written in C++ (C++17). The player intercepts incoming simulated network packets and decides to `accept` or `drop` each one. Health drops when packets are leaked (not handled before the next one arrives) or when malicious packets are accepted. Waves escalate difficulty over time.

## Building

Requires [Raylib](https://www.raylib.com/) (header + static lib). Windows-only.

MSVC (with Raylib in the include/lib path):
```bash
cl /std:c++17 /EHsc /O2 main.cpp raylib.lib gdi32.lib winmm.lib shell32.lib /Fe:sanjar.exe
```

MinGW/g++:
```bash
g++ -std=c++17 -O2 main.cpp -lraylib -lopengl32 -lgdi32 -lwinmm -o sanjar.exe
```

No build system. No tests. `InputHandler.h` and `ConsoleRenderer.h` are legacy console files — not compiled in the current build.

## Architecture

Rendering is done with Raylib (1280×720, 60 FPS, vsync). Game logic runs on a 20 TPS cadence inside the Raylib loop. `InputHandler.h` (legacy `_kbhit`/`_getch` thread) is unused; input is now read via `GetCharPressed()`/`IsKeyPressed()` on the main thread.

Everything lives in header-only classes (no `.cpp` besides `main.cpp`):

| File | Role |
|---|---|
| `Packet.h` | Plain data struct (`source_ip`, `port`, `protocol`, `payload`, `is_malicious`) |
| `PacketGenerator.h` | Spawns packets on a random timer; `heuristic_threat_score()` adds intentional noise ("lying AI") that grows with wave number |
| `GameManager.h` | Owns game state (score, health, wave, active packet); resolves `accept`/`drop` commands via `on_action()` |
| `InputHandler.h` | Legacy: background thread via `_kbhit`/`_getch` — not used in current Raylib build |
| `ConsoleRenderer.h` | Legacy: ANSI console renderer — not used in current Raylib build |
| `main.cpp` | Raylib loop at 60 FPS; game tick at 20 TPS; `DrawDesktop` → `DrawWallpaper` + `DrawTaskbar` |

### Game loop flow

1. `PacketGenerator::ready()` fires → `generate()` returns a new `Packet`; if `active_packet` is still set, the old one is marked leaked and 15 HP is deducted.
2. `InputHandler::poll()` drains the command queue; `GameManager::on_action("accept"|"drop")` resolves scoring/damage and clears `active_packet`.
3. `ConsoleRenderer` redraws only when `dirty` or ≥50 ms have elapsed since the last frame.

### Scoring

| Action | Outcome | Effect |
|---|---|---|
| `drop` malicious | Correct | +15 pts |
| `drop` safe | False positive | −5 pts |
| `accept` safe | Correct | +5 pts |
| `accept` malicious | Breach | −20 HP |
| Packet leaks (not handled) | — | −15 HP |

Wave increments every 10 packets. Packet spawn interval shrinks by `difficulty_multiplier = 1 + (wave-1) * 0.25`.

### Key design constraints

- `InputHandler` uses `<conio.h>` — **Windows-only**; the whole project is Windows-specific.
- `active_packet` is a raw pointer into `GameManager::active_storage_` (a value-type member). It is only valid while `active_storage_` is alive; never store it externally.
- The threat score from `heuristic_threat_score()` is deliberately unreliable (noise ±`10 + wave*5`). Do not treat it as ground truth.

## Token Efficiency & Behavior Rules

### Communication Style
- **Adopt "Caveman Mode" for prose:** Use minimal, primitive English (e.g., "File edit. Done.", "Bug fix. Ready.").
- **Zero Filler:** No politeness, no intros like "Certainly, I can help", no outros.
- **Concise Summaries:** If an explanation is needed, use bullet points and keep it under 2 lines.

### Code Editing Rules
- **Incremental Edits:** Never rewrite an entire file if you can use search-and-replace or edit specific functions.
- **Omit Redundant Comments:** Do not add docstrings or comments unless they explain complex security logic.
- **Diff-Preferred:** When possible, describe changes or provide only the modified code blocks rather than the full file content.
- **No Yapping during Tools:** When using shell or write tools, do not explain what you are doing. Just execute.

### Context Management
- **Focus:** Only read files that are directly related to the current task. 
- **Compact Memory:** If the conversation gets long, suggest a summary and a fresh start to clear the buffer.