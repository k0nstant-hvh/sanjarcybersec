#pragma once
#include <string>

struct Packet {
    std::string source_ip;
    int         port;
    std::string protocol;
    std::string payload;
    bool        is_malicious;
    bool        is_admin = false;
};
