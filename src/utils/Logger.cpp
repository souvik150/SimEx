#include "utils/Logger.h"

#include <memory>
#include <mutex>
#include <utility>

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "utils/Affinity.h"

namespace logging {

namespace {
constexpr std::size_t kDefaultQueueSize = 8192;
constexpr std::size_t kDefaultWorkerThreads = 1;
}  // namespace

class LoggerSingleton {
public:
    static LoggerSingleton& instance() {
        static LoggerSingleton singleton;
        return singleton;
    }

    void configure(const LoggerOptions& options) {
        std::lock_guard<std::mutex> lock(mutex_);
        options_ = options;
        configured_ = true;
    }

    spdlog::logger& get() {
        std::call_once(init_flag_, [this] { initialize(); });
        return *logger_;
    }

private:
    LoggerSingleton() = default;

    void initialize() {
        LoggerOptions opts;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            opts = configured_ ? options_ : LoggerOptions{};
        }

        const std::size_t queue_size = opts.queue_size == 0 ? kDefaultQueueSize : opts.queue_size;
        const std::size_t worker_threads = opts.worker_threads == 0 ? kDefaultWorkerThreads : opts.worker_threads;

        auto on_start = [affinity = opts.affinity] {
            if (!affinity.empty()) {
                cpu::setCurrentThreadAffinity(affinity);
            }
        };

        spdlog::init_thread_pool(queue_size, worker_threads, std::move(on_start));

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

    std::once_flag init_flag_;
    std::mutex mutex_;
    LoggerOptions options_;
    bool configured_ = false;
    std::shared_ptr<spdlog::logger> logger_;
};

void configureLogger(const LoggerOptions& options) {
    LoggerSingleton::instance().configure(options);
}

spdlog::logger& logger() {
    return LoggerSingleton::instance().get();
}

} // namespace logging
