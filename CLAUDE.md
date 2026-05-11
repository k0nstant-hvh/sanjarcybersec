# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**SANJAR** is a Raylib-based cybersecurity defense game written in C++17. It simulates a Windows-style desktop; the player double-clicks `sanjar.exe` on the desktop to open a terminal, types `start.exe` to begin, then intercepts simulated network packets by typing `accept` or `drop`. Health drops when packets leak or when malicious packets are accepted. Waves escalate difficulty over time.

## Building

Raylib is bundled in `raylib_lib/` — no separate install needed.

MSVC:
```
cl /std:c++17 /EHsc /O2 /I raylib_lib\include main.cpp raylib_lib\lib\raylib.lib gdi32.lib winmm.lib shell32.lib /Fe:sanjar.exe
```

MinGW/g++:
```
g++ -std=c++17 -O2 -I raylib_lib/include main.cpp -L raylib_lib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -o sanjar.exe
```

No build system. No tests. `InputHandler.h` and `ConsoleRenderer.h` are legacy console files — not compiled in the current build.

## Architecture

Everything lives in header-only classes (no `.cpp` besides `main.cpp`):

| File | Role |
|---|---|
| `Packet.h` | Plain data struct: `source_ip`, `port`, `protocol`, `payload`, `is_malicious`, `is_admin` |
| `PacketGenerator.h` | Spawns packets on a random timer; `heuristic_threat_score()` adds intentional noise ("Lying AI") that grows with wave number |
| `GameManager.h` | Owns game state (score, health, wave, active packet); resolves `accept`/`drop` via `on_action()` |
| `GuiWindow.h` | Draggable/closeable window widget; base class; `WinHit` enum for hit-test results |
| `VirtualTerminal.h` | `GuiWindow` subclass that hosts the game engine; handles keyboard input and 20 TPS game tick |
| `Icon.h` | Desktop icon struct + `DrawSingleIcon()` helper |
| `InputHandler.h` | Legacy: background thread via `_kbhit`/`_getch` — not used |
| `ConsoleRenderer.h` | Legacy: ANSI console renderer — not used |
| `main.cpp` | Raylib loop at 60 FPS; desktop simulation (wallpaper, icons, window stack, taskbar) |

### Desktop / Window stack

`main.cpp` manages a `WinVec` (`std::vector<std::unique_ptr<GuiWindow>>`):

- **Last element = focused** (rendered on top); clicking a window promotes it to the back of the vector.
- Double-clicking a desktop icon within 0.35 s spawns a new window:
  - `sanjar.exe` → `VirtualTerminal`
  - `guide.txt` / `settings.json` → plain `GuiWindow` with static body text
- Draw order: wallpaper → icons → windows (index 0 = bottom) → taskbar (always on top).
- Keyboard input is routed only to `windows.back()`.

### Game loop flow

1. Each Raylib frame calls `VirtualTerminal::UpdateGame()`, self-throttled to 20 TPS (50 ms).
2. `PacketGenerator::ready()` fires → `generate()` returns a new `Packet`; if `active_packet` is still set, the old one is marked leaked (−15 HP).
3. `VirtualTerminal::HandleInput()` reads `GetCharPressed()` / `IsKeyPressed()`; on Enter it calls `ProcessCommand()` → `GameManager::on_action("accept"|"drop")`.
4. Game over when `GameManager::health ≤ 0`; terminal shows final score and locks input.

### Scoring

| Action | Outcome | Effect |
|---|---|---|
| `drop` malicious | Correct | +15 pts |
| `drop` safe | False positive | −5 pts |
| `drop` admin (10.0.0.50) | Admin kicked | −50 pts |
| `accept` safe | Correct | +5 pts |
| `accept` malicious | Breach | −20 HP |
| Packet leaks (not handled) | — | −15 HP |

Wave increments every 10 packets. Spawn interval shrinks: `[500, 2500] / (1 + (wave-1)*0.25)` ms.

### Packet types

`PacketGenerator::generate()` picks a type by weighted roll:

| Type | `is_malicious` | Note |
|---|---|---|
| legitimate | false | Normal traffic |
| malicious | true | SQL/sys payload, port 4444 |
| web_attack | true | HTTP path traversal / SQLi |
| anomaly | true | Junk payload, port 53 HTTPS |
| junk | false | Random hex/base64 blob |
| admin | false, `is_admin=true` | `10.0.0.50` — scary payload, always safe; drop costs −50 pts |
| red_herring | false | Wrong port for protocol — looks suspicious, safe |
| zero_day | true | Trusted IP, clean payload — heuristic gives low score |

### Key design constraints

- `active_packet` is a raw pointer into `GameManager::active_storage_` (a value member). Never store it externally.
- `heuristic_threat_score()` is deliberately unreliable (noise ±`10 + wave*5`). Do not treat it as ground truth.
- Admin packets (`10.0.0.50`) look malicious by payload and heuristic but `is_malicious = false`; the penalty for dropping them is −50 pts, not HP damage.
- The entire project is Windows-specific (`<conio.h>`, `raylib.dll`, MSVC/MinGW only).

### Debug commands (in-game, undocumented)

These force the next generated packet to a specific type:

```
cmd_force_sql    → next packet is make_forced_sql()
cmd_force_admin  → next packet is make_admin()
cmd_force_junk   → next packet is make_junk()
```

## Token Efficiency & Behavior Rules

### Communication Style
- Use minimal prose: short sentences, bullet points, no filler intros/outros.
- If explanation is needed, keep it under 2 lines.

### Code Editing Rules
- Incremental edits only — never rewrite a full file when a targeted edit suffices.
- Omit comments unless they explain non-obvious security logic.
