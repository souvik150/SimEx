#pragma once

#include <cstdint>
#include <string>

struct SnapshotSettings {
    std::string shm_prefix = "/simex_book";
    uint32_t interval_ms = 50;
    uint32_t levels = 32;
};

struct AppConfig {
    std::string mcast_ip = "239.192.1.1";
    std::string mcast_iface = "lo";
    int mcast_port = 5001;
    bool use_std_map = false;
    SnapshotSettings snapshot;
};

AppConfig loadConfig(const std::string& path);
