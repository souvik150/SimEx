#include "utils/Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

inline std::string trim(const std::string& input) {
    const auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

inline bool startsWith(const std::string& text, char c) {
    return !text.empty() && text.front() == c;
}

} // namespace

std::vector<int> parseCpuList(const std::string& spec) {
    std::vector<int> cpus;
    size_t start = 0;
    while (start < spec.size()) {
        size_t end = spec.find(',', start);
        if (end == std::string::npos) {
            end = spec.size();
        }
        std::string token = trim(spec.substr(start, end - start));
        if (!token.empty()) {
            const auto dash = token.find('-');
            if (dash == std::string::npos) {
                cpus.push_back(std::stoi(token));
            } else {
                const int begin = std::stoi(token.substr(0, dash));
                const int finish = std::stoi(token.substr(dash + 1));
                if (finish >= begin) {
                    for (int cpu = begin; cpu <= finish; ++cpu) {
                        cpus.push_back(cpu);
                    }
                }
            }
        }
        start = end + 1;
    }
    return cpus;
}

AppConfig loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    AppConfig config;
    std::string line;
    std::string section = "network";
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || startsWith(line, '#') || startsWith(line, ';')) {
            continue;
        }
        if (startsWith(line, '[')) {
            const auto closing = line.find(']');
            if (closing != std::string::npos) {
                section = trim(line.substr(1, closing - 1));
            }
            continue;
        }

        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, pos));
        const std::string value = trim(line.substr(pos + 1));

        if (section == "network") {
            if (key == "mcast_ip") {
                config.mcast_ip = value;
            } else if (key == "mcast_iface") {
                config.mcast_iface = value;
            } else if (key == "mcast_port") {
                config.mcast_port = std::stoi(value);
            }
        } else if (section == "snapshot") {
            if (key == "shm_prefix") {
                config.snapshot.shm_prefix = value;
            } else if (key == "interval_ms") {
                config.snapshot.interval_ms = static_cast<uint32_t>(std::stoul(value));
            } else if (key == "levels") {
                config.snapshot.levels = static_cast<uint32_t>(std::stoul(value));
            }
        } else if (section == "orderbook") {
            if (key == "use_std_map") {
                config.use_std_map = (value == "1" || value == "true" || value == "TRUE");
            }
        } else if (section == "logging") {
            if (key == "queue_size") {
                config.logging.queue_size = static_cast<std::size_t>(std::stoul(value));
            } else if (key == "worker_threads") {
                config.logging.worker_threads = static_cast<std::size_t>(std::stoul(value));
            }
        } else if (section == "affinity") {
            if (key == "logging_cores") {
                config.affinity.logging_cores = parseCpuList(value);
            } else if (key == "engine_cores") {
                config.affinity.engine_cores = parseCpuList(value);
            }
        }
    }

    return config;
}
