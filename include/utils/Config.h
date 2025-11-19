#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SnapshotSettings {
    std::string shm_prefix = "/simex_book";
    uint32_t interval_ms = 50;
    uint32_t levels = 32;
};

struct LoggingSettings {
    std::size_t queue_size = 8192;
    std::size_t worker_threads = 1;
};

struct AffinitySettings {
    std::vector<int> logging_cores;
    std::vector<int> engine_cores;
};

struct AppConfig {
    std::string mcast_ip = "239.192.1.1";
    std::string mcast_iface = "lo";
    int mcast_port = 5001;
    bool use_std_map = false;
    SnapshotSettings snapshot;
    LoggingSettings logging;
    AffinitySettings affinity;
};

AppConfig loadConfig(const std::string& path);
