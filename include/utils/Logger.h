#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <vector>

namespace logging {

struct LoggerOptions {
    std::size_t queue_size = 8192;
    std::size_t worker_threads = 1;
    std::vector<int> affinity;
};

void configureLogger(const LoggerOptions& options);
spdlog::logger& logger();

}
