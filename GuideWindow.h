#pragma once
#include "GuiWindow.h"
#include <algorithm>
#include <string>
#include <vector>

class GuideWindow : public GuiWindow {
    struct Entry { Color color; std::string text; };
    std::vector<Entry>   entries_;
    mutable int          scroll_ = 0;

    static constexpr float FS = 13.0f;
    static constexpr float LH = 18.0f;

public:
    GuideWindow(float x, float y)
        : GuiWindow("guide.pdf", x, y, "", 570.0f, 420.0f)
    {
        auto add = [&](Color c, const char* t){ entries_.push_back({c, t}); };

        add({0,   200, 255, 255}, "S.A.N.J.A.R. DEFENSE GUIDE  v2.0");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "XSS — Cross-Site Scripting");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({200, 200, 200, 255}, "Tags — DROP immediately:");
        add({210,  80,  80, 255}, "  <script>...</script>");
        add({210,  80,  80, 255}, "  <img src=x onerror=...>");
        add({210,  80,  80, 255}, "  <svg onload=...>  /  <iframe src=...>");
        add({200, 200, 200, 255}, "  Any attribute: alert()  steal()  cmd()");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "SQLi — SQL Injection");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({200, 200, 200, 255}, "Common bypasses — DROP on sight:");
        add({210,  80,  80, 255}, "  ' OR 1=1 --");
        add({210,  80,  80, 255}, "  UNION SELECT  /  UNION ALL SELECT");
        add({210,  80,  80, 255}, "  DROP TABLE  /  DELETE FROM");
        add({210,  80,  80, 255}, "  INSERT INTO  /  UPDATE ... SET hash=''");
        add({210,  80,  80, 255}, "  SELECT * FROM  (data exfiltration)");
        add({200, 200, 200, 255}, "  Suffixes:  ; --   WHERE 1=1--   LIMIT 1--");
        add({200, 200, 200, 255}, "  INTO OUTFILE '/tmp/pwn'");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "RCE — Remote Code Execution");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({200, 200, 200, 255}, "Dangerous system calls — DROP:");
        add({210,  80,  80, 255}, "  nc -e /bin/bash        (reverse shell)");
        add({210,  80,  80, 255}, "  rm -rf  /  chmod 777  /  wget  /  curl");
        add({210,  80,  80, 255}, "  cat /etc/shadow  /  cat /etc/passwd");
        add({210,  80,  80, 255}, "  cat /root/.ssh/id_rsa");
        add({210,  80,  80, 255}, "  sudo  /  bash  /  sh  /  python -c");
        add({200, 200, 200, 255}, "  Port 4444  =  likely reverse shell");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "SPECIAL CASES");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({ 80, 210,  80, 255}, "ADMIN PACKETS — always ACCEPT:");
        add({ 80, 210,  80, 255}, "  Source IP:  10.0.0.50   Protocol: SSH (port 22)");
        add({200, 200, 200, 255}, "  Payload looks malicious — that is intentional.");
        add({200, 200, 200, 255}, "  Dropping costs -50 pts (no HP damage).");
        add({255, 255, 255, 255}, "");
        add({130, 170, 255, 255}, "ZERO-DAYS — Trusted IPs, innocent payload:");
        add({130, 170, 255, 255}, "  IPs: 8.8.8.8  /  1.1.1.1  /  104.16.x");
        add({130, 170, 255, 255}, "  Ports: 443 / 80 / 53 / 22 / 25  — looks clean");
        add({130, 170, 255, 255}, "  Heuristic score will be LOW.  DROP anyway.");
        add({255, 255, 255, 255}, "");
        add({130, 170, 255, 255}, "RED HERRINGS — wrong port for protocol:");
        add({130, 170, 255, 255}, "  HTTPS on 8080/8443,  DNS on 9999,  SSH on 2222");
        add({130, 170, 255, 255}, "  Looks suspicious — actually safe.  ACCEPT.");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "SAFE TRAFFIC — Accept these:");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({ 80, 210,  80, 255}, "  HTTP   GET /index.html          (port 80)");
        add({ 80, 210,  80, 255}, "  HTTPS  TLS ClientHello          (port 443)");
        add({ 80, 210,  80, 255}, "  DNS    QUERY A example.com      (port 53)");
        add({ 80, 210,  80, 255}, "  SSH    SSH-2.0-OpenSSH          (port 22)");
        add({ 80, 210,  80, 255}, "  SMTP   MAIL FROM / DATA          (port 25)");
        add({200, 200, 200, 255}, "  Junk (random hex/base64)  =  not malicious");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "SCORING TABLE");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({ 80, 210,  80, 255}, "  drop  malicious           →  +15 pts");
        add({ 80, 210,  80, 255}, "  accept safe               →  +5  pts");
        add({210,  80,  80, 255}, "  drop  safe (false pos.)   →  -5  pts");
        add({210,  80,  80, 255}, "  accept malicious          →  -20 HP");
        add({210,  80,  80, 255}, "  drop  admin (10.0.0.50)   →  -50 pts");
        add({210,  80,  80, 255}, "  packet leaked (no action) →  -15 HP");
        add({255, 255, 255, 255}, "");
        add({200, 200, 200, 255}, "  Wave increments every 10 packets seen.");
        add({200, 200, 200, 255}, "  Heuristic noise: ±(10 + wave*5) — unreliable!");
        add({255, 255, 255, 255}, "");

        add({220, 180,  40, 255}, "COMMANDS");
        add({45,   50,  65, 255}, "────────────────────────────────────────────────");
        add({175, 175, 175, 255}, "  accept  |  drop  |  quit  |  status  |  help");
        add({110, 110, 110, 255}, "  cmd_force_sql  |  cmd_force_admin  |  cmd_force_junk");
        add({255, 255, 255, 255}, "");
        add({110, 110, 110, 255}, "  Scroll this window with the mouse wheel.");
    }

    void HandleInput() override {
        int w = -(int)GetMouseWheelMove();
        if (w != 0 && CheckCollisionPointRec(GetMousePosition(), bounds)) {
            scroll_ += w * 3;
            int max_s = std::max(0, (int)entries_.size() - visible_lines());
            scroll_   = std::max(0, std::min(scroll_, max_s));
        }
    }

protected:
    void DrawContent(bool /*focused*/) const override {
        const float CX = bounds.x + 8.0f;
        const float CY = bounds.y + WIN_TITLE_H + 6.0f;
        const float CW = bounds.width  - 16.0f;
        const float CH = bounds.height - WIN_TITLE_H - 10.0f;

        DrawRectangle((int)CX, (int)CY, (int)CW, (int)CH, {6, 9, 16, 255});
        BeginScissorMode((int)CX, (int)CY, (int)CW, (int)CH);

        int end = std::min((int)entries_.size(), scroll_ + visible_lines() + 1);
        for (int i = scroll_; i < end; ++i) {
            float y = CY + 6.0f + (float)(i - scroll_) * LH;
            DrawText(entries_[i].text.c_str(), (int)(CX + 8), (int)y,
                     (int)FS, entries_[i].color);
        }

        EndScissorMode();

        if ((int)entries_.size() > visible_lines()) {
            const char* hint = "mouse wheel to scroll";
            DrawText(hint,
                     (int)(bounds.x + bounds.width - MeasureText(hint, 11) - 8),
                     (int)(bounds.y + bounds.height - 16), 11, {60, 70, 100, 200});
        }
    }

private:
    int visible_lines() const {
        return (int)((bounds.height - WIN_TITLE_H - 10.0f) / LH);
    }
};
