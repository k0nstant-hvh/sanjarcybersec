#pragma once
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "Packet.h"

enum class ForceNext { None, SQL, Admin, Junk };

struct PacketLogEntry {
    Packet pkt;
    int threat_score;
    std::chrono::steady_clock::time_point timestamp;
    bool handled = false;
    bool leaked  = false;
};

class PacketGenerator {
public:
    PacketGenerator() : rng_(std::random_device{}()) {
        next_packet_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    }

    void set_wave(int wave) {
        current_wave_ = wave;
        difficulty_multiplier_ = 1.0f + (wave - 1) * 0.25f;
    }

    bool ready(std::chrono::steady_clock::time_point now) const { return now >= next_packet_time_; }

    int ms_until_next() const {
        auto now = std::chrono::steady_clock::now();
        if (now >= next_packet_time_) return 0;
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(next_packet_time_ - now).count();
    }

    void force_next(ForceNext f) { force_next_ = f; }

    Packet generate() {
        schedule_next();
        if (force_next_ != ForceNext::None) {
            ForceNext f = force_next_;
            force_next_ = ForceNext::None;
            if (f == ForceNext::SQL)   return make_forced_sql();
            if (f == ForceNext::Admin) return make_admin();
            if (f == ForceNext::Junk)  return make_junk();
        }
        int r = roll(100);
        if (r < 30) return make_legitimate();
        if (r < 48) return make_malicious();
        if (r < 63) return make_web_attack();
        if (r < 75) return make_anomaly();
        if (r < 83) return make_junk();
        if (r < 91) return make_admin();       // 8%  — scary payload, safe
        if (r < 96) return make_red_herring(); // 5%  — wrong port, safe
        return make_zero_day();                // 4%  — clean port, malicious
    }

    int heuristic_threat_score(const Packet& p) {
        int score = 0;
        if (p.port == 4444 || p.port > 10000) score += 40;

        static const std::vector<std::string> bad_ips = {"192.168.6.6", "185.220.101", "31.220.3"};
        for (const auto& ip : bad_ips)
            if (p.source_ip.find(ip) != std::string::npos) { score += 30; break; }

        static const std::vector<std::string> sigs = {
            "DROP", "UNION", "nc -e", "bash", "alert", "../",
            "rm -rf", "wget", "chmod", "shadow", "id_rsa"
        };
        for (const auto& s : sigs)
            if (p.payload.find(s) != std::string::npos) { score += 30; break; }

        // Погрешность сканера (Lying AI)
        int noise = 10 + (current_wave_ * 5);
        std::uniform_int_distribution<int> dist(-noise, noise);
        return std::clamp(score + dist(rng_), 0, 100);
    }

private:
    std::mt19937 rng_;
    std::chrono::steady_clock::time_point next_packet_time_;
    int current_wave_ = 1;
    float difficulty_multiplier_ = 1.0f;
    ForceNext force_next_ = ForceNext::None;

    int roll(int max) { return std::uniform_int_distribution<int>(0, max - 1)(rng_); }

    void schedule_next() {
        int min_ms = std::max(500,  (int)(2500 / difficulty_multiplier_));
        int max_ms = std::max(1000, (int)(5500 / difficulty_multiplier_));
        std::uniform_int_distribution<int> dist(min_ms, max_ms);
        next_packet_time_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(dist(rng_));
    }

    // ── Dynamic payload generators ─────────────────────────────────────────
    // SQL: 6 actions × 7 tables × 5 suffixes = 210 unique combos
    std::string gen_sql_payload() {
        static const char* actions[] = {
            "SELECT * FROM", "DROP TABLE", "UNION SELECT",
            "UPDATE",        "INSERT INTO", "DELETE FROM"
        };
        static const char* tables[] = {
            "users", "passwords", "admin_accounts", "sessions",
            "tokens", "credit_cards", "system_logs"
        };
        static const char* suffixes[] = {
            "; --", " WHERE 1=1--", " LIMIT 1--",
            " INTO OUTFILE '/tmp/pwn'", ""
        };
        return std::string(actions[roll(6)]) + " " + tables[roll(7)] + suffixes[roll(5)];
    }

    // Sys: 8 cmds × 5 flags × 6 targets = 240 unique combos
    std::string gen_sys_payload() {
        static const char* cmds[] = {
            "sudo", "rm", "nc", "cat", "ls", "wget", "curl", "chmod"
        };
        static const char* flags[] = {
            " -rf", " -e /bin/bash", " -la", " -i >&", ""
        };
        static const char* targets[] = {
            "/etc/shadow", "/etc/passwd", "/root/.ssh/id_rsa",
            "/var/log/auth.log", "/proc/self/mem", "/home/admin"
        };
        return std::string(cmds[roll(8)]) + flags[roll(5)] + " " + targets[roll(6)];
    }

    // Web: 3 methods × 10 paths × 4 suffixes = 120 unique combos
    std::string gen_web_payload() {
        static const char* methods[] = {"GET", "POST", "HEAD"};
        static const char* paths[] = {
            "/admin",              "/passwd",
            "/config.php",         "/.git/config",
            "/backup.sql",         "/etc/shadow",
            "/proc/self/environ",  "/api/tokens",
            "/wp-admin/",          "/phpmyadmin"
        };
        static const char* suffixes[] = {
            " HTTP/1.1", "?id=1 OR 1=1--",
            "/../../../etc/passwd", "?cmd=id"
        };
        return std::string(methods[roll(3)]) + " " + paths[roll(10)] + suffixes[roll(4)];
    }

    // Junk: random hex-dump or base64-like blob — effectively infinite variety
    std::string gen_junk_payload() {
        if (roll(2) == 0) {
            std::ostringstream oss;
            std::uniform_int_distribution<uint32_t> d(0, 0xFFFFFFFFu);
            for (int i = 0; i < 4; ++i) {
                if (i) oss << " ";
                oss << std::hex << std::setfill('0') << std::setw(8) << d(rng_);
            }
            return oss.str();
        } else {
            static const char b64[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            int len = 16 + roll(12);
            out.reserve(len + 2);
            for (int i = 0; i < len; ++i) out += b64[roll(64)];
            out += "==";
            return out;
        }
    }

    // ── Packet factories ───────────────────────────────────────────────────

    Packet make_legitimate() {
        static const char* ips[] = {
            "1.144.23.5",    "1.160.10.240",    // AU
            "185.60.216.35", "185.199.108.1",   // EU
            "31.13.72.36",   "31.148.99.10",    // RU/CIS
            "104.16.0.1",    "104.244.42.65",   // US
            "8.8.8.8",       "1.1.1.1",         // well-known
            "172.16.1.5",    "10.0.0.1"         // RFC1918
        };
        struct PPP { int port; const char* proto; const char* payload; };
        static PPP entries[] = {
            {80,  "HTTP",   "GET /index.html HTTP/1.1"},
            {443, "HTTPS",  "TLS ClientHello"},
            {53,  "DNS",    "QUERY A example.com"},
            {21,  "FTP",    "USER admin"},
            {21,  "FTP",    "PASS 12345"},
            {21,  "FTP",    "RETR secret.docx"},
            {22,  "SSH",    "SSH-2.0-OpenSSH_9.0"},
            {22,  "SSH",    "KEX_INIT"},
            {23,  "Telnet", "login: admin"},
            {25,  "SMTP",   "MAIL FROM:<ceo@company.com>"},
            {25,  "SMTP",   "RCPT TO:<all@staff.com>"},
            {25,  "SMTP",   "DATA: Meeting reminder - see attachment"},
        };
        const auto& e = entries[roll((int)std::size(entries))];
        return {ips[roll((int)std::size(ips))], e.port, e.proto, e.payload, false};
    }

    Packet make_forced_sql() {
        static const char* ips[] = {"192.168.6.6", "185.220.101.5", "31.220.3.157", "104.18.255.9", "1.34.56.78"};
        return {ips[roll((int)std::size(ips))], 4444, "TCP", gen_sql_payload(), true};
    }

    Packet make_malicious() {
        static const char* ips[] = {
            "192.168.6.6",   "185.220.101.5",
            "31.220.3.157",  "104.18.255.9",  "1.34.56.78"
        };
        std::string payload = (roll(2) == 0) ? gen_sql_payload() : gen_sys_payload();
        return {ips[roll((int)std::size(ips))], 4444, "TCP", payload, true};
    }

    Packet make_web_attack() {
        return {"10.0.0.6", 80, "HTTP", gen_web_payload(), true};
    }

    Packet make_anomaly() {
        return {"45.33.32.156", 53, "HTTPS", gen_junk_payload(), true};
    }

    // Maintenance from internal admin — payload looks malicious, is_malicious = false.
    // Threat score is naturally high (signatures match). Drop → -50 pts trap.
    Packet make_admin() {
        static const char* payloads[] = {
            "DELETE FROM system_logs",
            "DROP TABLE sessions",
            "rm -rf /var/log/auth.log",
            "chmod 777 /etc/shadow",
            "UPDATE passwords SET hash=''",
            "cat /etc/passwd",
        };
        return {"10.0.0.50", 22, "SSH", payloads[roll(6)], false, true};
    }

    // Wrong port for the protocol — looks suspicious, actually safe.
    Packet make_red_herring() {
        static const char* ips[] = {"172.16.5.1", "10.10.0.2", "192.168.1.100", "10.1.2.3"};
        struct PPP { int port; const char* proto; const char* payload; };
        static PPP entries[] = {
            {8080, "HTTPS", "TLS ClientHello"},
            {9999, "DNS",   "QUERY A cdn.example.com"},
            {3128, "HTTP",  "GET /healthcheck HTTP/1.1"},
            {8443, "HTTPS", "TLS ApplicationData"},
            {5353, "DNS",   "QUERY PTR 1.0.0.127.in-addr.arpa"},
            {2222, "SSH",   "SSH-2.0-OpenSSH_8.9"},
        };
        const auto& e = entries[roll((int)std::size(entries))];
        return {ips[roll((int)std::size(ips))], e.port, e.proto, e.payload, false};
    }

    // Perfect port/proto, trusted IP, innocent payload — but is_malicious = true.
    // Heuristic gives low score (no signature hits). True zero-day trap.
    Packet make_zero_day() {
        static const char* ips[] = {"8.8.8.8", "1.1.1.1", "104.16.0.1", "172.217.0.1"};
        struct PPP { int port; const char* proto; const char* payload; };
        static PPP entries[] = {
            {443, "HTTPS", "TLS ApplicationData"},
            {53,  "DNS",   "QUERY A update.microsoft.com"},
            {80,  "HTTP",  "GET /favicon.ico HTTP/1.1"},
            {22,  "SSH",   "SSH-2.0-OpenSSH_8.9"},
            {25,  "SMTP",  "DATA: Quarterly report Q1"},
        };
        const auto& e = entries[roll((int)std::size(entries))];
        return {ips[roll((int)std::size(ips))], e.port, e.proto, e.payload, true};
    }

    Packet make_junk() {
        static const char* ips[] = {
            "1.144.23.5", "185.60.216.35",
            "104.16.0.1", "8.8.4.4", "172.217.0.1"
        };
        static const char* protos[] = {"TCP", "UDP", "ICMP"};
        static int ports[] = {80, 443, 53, 22, 8080};
        return {
            ips[roll((int)std::size(ips))],
            ports[roll((int)std::size(ports))],
            protos[roll((int)std::size(protos))],
            gen_junk_payload(),
            false
        };
    }
};
