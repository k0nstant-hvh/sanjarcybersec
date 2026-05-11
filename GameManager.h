#pragma once
#include <string>
#include <algorithm>
#include "Packet.h"

struct GameSettings {
    float difficulty = 1.5f;
    float spawn_rate = 0.8f;
    bool  dev_mode   = false;
};

enum class ActionResult {
    BlockedThreat,    // drop  + malicious → correct   (+15 pts)
    FalsePositive,    // drop  + safe      → mistake   ( -5 pts)
    AcceptedSafe,     // accept + safe     → correct   ( +5 pts)
    AcceptedThreat,   // accept + malicious→ breach    (-20  hp)
    AdminBlocked,     // drop  + admin     → big error (-50 pts)
    ExitCommand,      // "quit" typed      → graceful exit
    UnknownCommand,
};

class GameManager {
public:
    int     score         = 0;
    int     health        = 100;
    int     current_wave  = 1;
    bool    dev_mode      = false;
    int     packets_seen  = 0;
    bool    running       = true;
    Packet* active_packet = nullptr;   // points to active_storage_, or null

    static constexpr int WAVE_EVERY   = 10;
    static constexpr int LEAK_DAMAGE  = 15;
    static constexpr int MSG_DURATION = 40;   // 2 s at 20 TPS

    // ── Action feedback (displayed by ConsoleRenderer::draw_action_message) ──
    std::string last_action_msg;
    bool        msg_positive = true;
    int         msg_timer    = 0;

    void set_message(const std::string& msg, bool positive) {
        last_action_msg = msg;
        msg_positive    = positive;
        msg_timer       = MSG_DURATION;
    }

    void tick_message() { if (msg_timer > 0) --msg_timer; }

    bool is_alive() const { return running; }

    void apply_damage(int dmg) {
        health -= dmg;
        if (health <= 0) { health = 0; running = false; }
    }

    void add_score(int pts) { score += pts; }

    // Called when a new packet arrives from the generator.
    // Returns true if the previous active_packet leaked (damage already applied).
    bool arrive(const Packet& p) {
        bool leaked = (active_packet != nullptr);
        if (leaked) apply_damage(LEAK_DAMAGE);

        active_storage_ = p;
        active_packet   = &active_storage_;

        ++packets_seen;
        if (packets_seen % WAVE_EVERY == 0) ++current_wave;
        return leaked;
    }

    // Resolves the current active_packet against the player's command.
    // Clears active_packet on a valid command. Returns UnknownCommand if
    // no active packet or unrecognized input (does NOT clear active_packet).
    ActionResult on_action(const std::string& cmd) {
        if (cmd == "quit") { running = false; return ActionResult::ExitCommand; }
        if (!active_packet) return ActionResult::UnknownCommand;

        const Packet& pkt = *active_packet;
        ActionResult  res = ActionResult::UnknownCommand;

        std::string src = pkt.source_ip + ":" + std::to_string(pkt.port);

        if (cmd == "drop") {
            if (pkt.is_admin) {
                score = std::max(0, score - 50);
                set_message("ADMIN KICKED!       >> " + src + "  [-50 PTS]", false);
                res = ActionResult::AdminBlocked;
            } else if (pkt.is_malicious) {
                add_score(15);
                set_message("THREAT PURGED       >> " + src + "  [+15 PTS]", true);
                res = ActionResult::BlockedThreat;
            } else {
                score = std::max(0, score - 5);
                set_message("FALSE POSITIVE      >> " + src + "  [-5 PTS]",  false);
                res = ActionResult::FalsePositive;
            }
        } else if (cmd == "accept") {
            if (!pkt.is_malicious) {
                add_score(5);
                set_message("PACKET ACCEPTED     >> " + src + "  [+5 PTS]",  true);
                res = ActionResult::AcceptedSafe;
            } else {
                apply_damage(20);
                set_message("BREACH DETECTED!    >> " + src + "  [-20 HP]",  false);
                res = ActionResult::AcceptedThreat;
            }
        }

        if (res != ActionResult::UnknownCommand) active_packet = nullptr;
        return res;
    }

private:
    Packet active_storage_;  // owned buffer; active_packet == &active_storage_ or nullptr
};
