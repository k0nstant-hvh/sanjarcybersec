#include <chrono>
#include <thread>
#include <deque>
#include <algorithm>

#include "Packet.h"
#include "GameManager.h"
#include "PacketGenerator.h"
#include "ConsoleRenderer.h"
#include "InputHandler.h"

static constexpr int   TARGET_TPS    = 20;
static constexpr auto  TICK_DURATION = std::chrono::milliseconds(1000 / TARGET_TPS);
static constexpr int   MAX_LOG_LINES = 12;
static constexpr int   RENDER_MS     = 50;

int main() {
    ConsoleRenderer::enable_colors();
    ConsoleRenderer::clear();

    GameManager                gm;
    PacketGenerator            gen;
    InputHandler               input;
    std::deque<PacketLogEntry> log;

    bool        dirty          = true;
    std::string last_input_buf;
    auto        last_render    = std::chrono::steady_clock::time_point{};

    while (gm.is_alive()) {
        auto frame_start = std::chrono::steady_clock::now();

        // ── Message timer ─────────────────────────────────────────────────
        bool was_ticking = (gm.msg_timer > 0);
        gm.tick_message();
        if (was_ticking && gm.msg_timer == 0) dirty = true;   // message just expired

        // ── Packet spawn ──────────────────────────────────────────────────
        if (gen.ready(frame_start)) {
            Packet pkt    = gen.generate();
            int    threat = gen.heuristic_threat_score(pkt);

            if (gm.active_packet != nullptr) {
                std::string leaked_src =
                    gm.active_packet->source_ip + ":" +
                    std::to_string(gm.active_packet->port);

                auto it = std::find_if(log.begin(), log.end(),
                    [](const PacketLogEntry& e){ return !e.handled && !e.leaked; });
                if (it != log.end()) it->leaked = true;

                gm.arrive(pkt);
                gm.set_message("PACKET LEAKED!      >> " + leaked_src + "  [-15 HP]", false);
            } else {
                gm.arrive(pkt);
            }

            log.push_front({ pkt, threat, frame_start, false, false });
            if (log.size() > MAX_LOG_LINES) log.pop_back();
            gen.set_wave(gm.current_wave);
            dirty = true;
        }

        // ── Command input ─────────────────────────────────────────────────
        std::string cmd;
        if (input.poll(cmd)) {
            if (gm.active_packet != nullptr) {
                Packet       pkt_snap = *gm.active_packet;
                ActionResult res      = gm.on_action(cmd);   // sets gm.last_action_msg

                if (res != ActionResult::UnknownCommand) {
                    auto it = std::find_if(log.begin(), log.end(),
                        [](const PacketLogEntry& e){ return !e.handled && !e.leaked; });
                    if (it != log.end()) it->handled = true;
                } else {
                    gm.set_message("UNKNOWN CMD: accept / drop / quit", false);
                }
            } else if (cmd == "quit") {
                gm.on_action(cmd);
            } else {
                gm.set_message("NO ACTIVE PACKET IN QUEUE", false);
            }
            dirty = true;
        }

        // ── Input buffer change detection ─────────────────────────────────
        std::string cur_buf = input.current_buffer();
        if (cur_buf != last_input_buf) { last_input_buf = cur_buf; dirty = true; }

        // ── Render gate ───────────────────────────────────────────────────
        auto ms_since = std::chrono::duration_cast<std::chrono::milliseconds>(
                            frame_start - last_render).count();

        if (dirty || ms_since >= RENDER_MS) {
            ConsoleRenderer::begin_frame();
            ConsoleRenderer::draw_header(gm);
            ConsoleRenderer::draw_packet_log(log, gen.ms_until_next());
            ConsoleRenderer::draw_action_message(gm);
            ConsoleRenderer::draw_input_area(cur_buf);
            ConsoleRenderer::end_frame();
            dirty       = false;
            last_render = frame_start;
        }

        // ── Frame cap ─────────────────────────────────────────────────────
        auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < TICK_DURATION)
            std::this_thread::sleep_for(TICK_DURATION - elapsed);
    }

    input.stop();
    ConsoleRenderer::draw_game_over(gm);
    int k; do { k = _getch(); } while (k != 27 && k != '\r');
    return 0;
}
