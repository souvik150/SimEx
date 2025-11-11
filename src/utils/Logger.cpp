#include "utils/Logger.h"

#include <memory>
#include <mutex>

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace logging {

class LoggerSingleton {
public:
    static LoggerSingleton& instance() {
        static LoggerSingleton singleton;
        return singleton;
    }

    spdlog::logger& get() {
        return *logger_;
    }

private:
    LoggerSingleton() {
        constexpr std::size_t kQueueSize = 8192;
        constexpr std::size_t kWorkerThreads = 1;
        spdlog::init_thread_pool(kQueueSize, kWorkerThreads);

        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %s:%# %! %v");

        logger_ = std::make_shared<spdlog::async_logger>(
            "simex",
            sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        logger_->set_level(spdlog::level::info);
        logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %s:%# %! %v");
        spdlog::register_logger(logger_);
    }

    std::shared_ptr<spdlog::logger> logger_;
};

spdlog::logger& logger() {
    return LoggerSingleton::instance().get();
}

} // namespace logging
