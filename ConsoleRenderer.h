#pragma once
#include <cstdio>
#include <deque>
#include <string>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
#include "GameManager.h"
#include "PacketGenerator.h"

#define C_RESET  "\033[0m"
#define C_BOLD   "\033[1m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_CYAN   "\033[36m"
#define C_GRAY   "\033[90m"
#define C_BWHITE "\033[1;97m"

class ConsoleRenderer {
public:
    static void enable_colors() {
#ifdef _WIN32
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(h, &mode);
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        SetConsoleOutputCP(65001);
#endif
        std::fputs("\033[?25l", stdout);
    }

    static void clear() { std::fputs("\033[2J\033[H", stdout); }
    static void begin_frame() { std::fputs("\033[H", stdout); }
    static void end_frame() { std::fputs("\033[J", stdout); std::fflush(stdout); }

    static void draw_header(const GameManager& gm) {
        std::fputs(C_CYAN C_BOLD, stdout);
        std::puts("================================================================");
        std::puts("   S . A . N . J . A . R .   |   Cybersecurity Defense System ");
        std::puts("================================================================" C_RESET);

        std::printf("  " C_BOLD C_YELLOW "[ W A V E  %02d ]" C_RESET "\n", gm.current_wave);
        
        const char* hp_col = (gm.health > 60) ? C_GREEN : (gm.health > 30) ? C_YELLOW : C_RED;
        // Пофикшено: убран лишний %
        std::printf("  Score: " C_BOLD "%-6d" C_RESET "  Health: %s%d%%" C_RESET "      \n", 
                    gm.score, hp_col, gm.health);
        std::puts("----------------------------------------------------------------");
    }

    static void draw_packet_log(const std::deque<PacketLogEntry>& log, int next_ms) {
        std::printf(C_BOLD "  INCOMING PACKET STREAM" C_RESET "       next packet in: " C_YELLOW "%4dms" C_RESET "  \n", next_ms);
        std::puts("  ----------------------------------------------------------------");
        std::printf("  %-4s %-16s %-6s %-6s %-8s %-25s %-10s\n", " ", "SOURCE IP", "PORT", "PROTO", "THREAT", "PAYLOAD", "STATUS");
        std::puts("  ----------------------------------------------------------------");

        bool active_found = false;
        for (const auto& e : log) {
            bool is_active = (!e.handled && !e.leaked && !active_found);
            if (is_active) active_found = true;

            std::string payload = e.pkt.payload;
            if (payload.size() > 22) payload = payload.substr(0, 19) + "...";

            const char* marker = is_active ? C_BOLD C_YELLOW "[>]" C_RESET : (e.leaked ? C_RED "[!]" C_RESET : "---");
            const char* status = e.leaked ? "[LEAKED]" : (e.handled ? "[handled]" : "         ");
            
            const char* row_col = is_active ? C_BWHITE : (e.handled ? C_GRAY : C_RESET);
            const char* thr_col = (e.threat_score > 70) ? C_RED : (e.threat_score > 30) ? C_YELLOW : C_GREEN;

            // Сборка строки с принудительной очисткой конца (затирает любые артефакты)
            std::printf("  %s %s%-16s %-6d %-6s %s[%3d%%]%s %-25s %-10s" C_RESET " \n",
                        marker, row_col, e.pkt.source_ip.c_str(), e.pkt.port, 
                        e.pkt.protocol.c_str(), thr_col, e.threat_score, row_col, 
                        payload.c_str(), status);
        }
    }

    static void draw_action_message(const GameManager& gm) {
        std::puts("  ----------------------------------------------------------------");
        if (gm.msg_timer > 0 && !gm.last_action_msg.empty()) {
            const char* col = gm.msg_positive ? C_GREEN : C_RED;
            std::printf("  %s" C_BOLD "  >> %-56s" C_RESET "  \n", col, gm.last_action_msg.c_str());
        } else {
            std::puts("                                                                  "); 
        }
    }

    static void draw_input_area(const std::string& buf) {
        std::printf("  " C_BOLD C_CYAN "COMMAND" C_RESET " " C_BOLD ">" C_RESET " %-20s  " C_GRAY "[accept / drop]" C_RESET "          \n", (buf + "_").c_str());
    }

    static void draw_game_over(const GameManager& gm) {
        std::fputs("\033[?25h", stdout);
        clear();
        std::fputs(C_RED C_BOLD, stdout);
        std::puts("\n\n  +====================================+");
        std::puts("  |    *** SYSTEM COMPROMISED *** |");
        std::puts("  +====================================+" C_RESET);
        std::printf("\n  Final Score      : %d\n", gm.score);
        std::printf("  Waves Survived   : %d\n", gm.current_wave - 1);
        std::puts("\n  Press ENTER to exit...");
    }
};