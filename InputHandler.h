#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <conio.h>   // _kbhit, _getch  (MSVC / Windows only)

class InputHandler {
public:
    InputHandler() : running_(true), thread_(&InputHandler::loop, this) {}

    ~InputHandler() {
        running_ = false;
        if (thread_.joinable()) thread_.detach();
    }

    void stop() { running_ = false; if (thread_.joinable()) thread_.join(); }

    InputHandler(const InputHandler&)            = delete;
    InputHandler& operator=(const InputHandler&) = delete;

    // Call from game thread: returns true + pops one command if available.
    bool poll(std::string& out) {
        std::lock_guard<std::mutex> g(mx_);
        if (cmds_.empty()) return false;
        out = std::move(cmds_.front());
        cmds_.pop();
        return true;
    }

    // Live view of what the player is currently typing (for renderer).
    std::string current_buffer() {
        std::lock_guard<std::mutex> g(mx_);
        return buf_;
    }

private:
    std::queue<std::string> cmds_;
    std::string             buf_;
    std::mutex              mx_;
    std::thread             thread_;
    std::atomic<bool>       running_;

    void loop() {
        while (running_) {
            if (!_kbhit()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            int ch = _getch();

            // Extended key sequence (arrow keys, F-keys etc.) — discard both bytes
            if (ch == 0 || ch == 0xE0) { _getch(); continue; }

            std::lock_guard<std::mutex> g(mx_);
            if (ch == '\r') {                          // Enter — submit
                if (!buf_.empty()) { cmds_.push(buf_); buf_.clear(); }
            } else if ((ch == 8 || ch == 127) && !buf_.empty()) { // Backspace
                buf_.pop_back();
            } else if (ch == '1') { cmds_.push("cmd_force_sql");   // director hotkeys — buf_ untouched
            } else if (ch == '2') { cmds_.push("cmd_force_admin");
            } else if (ch == '3') { cmds_.push("cmd_force_junk");
            } else if (ch >= 32 && ch <= 126 && buf_.size() < 16) {
                buf_ += static_cast<char>(ch);
            }
        }
    }
};
