#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>

#include "ingress/McastSocket.h"
#include "ingress/WireOrder.h"
#include "utils/Config.h"
#include "utils/LogMacros.h"

#ifdef PROJECT_ROOT
constexpr const char* kConfigPath = PROJECT_ROOT "/config/app.ini";
#else
constexpr const char* kConfigPath = "config/app.ini";
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr InstrumentToken kInstrumentToken = 26000;
constexpr double kClosingPrice = 1518.20;          // INR
constexpr double kSigma = 0.005;                   // ≈0.5% std dev
constexpr double kDeviationClamp = 0.05;           // clamp ±5%
constexpr Qty kMinQty = 10;
constexpr Qty kMaxQty = 200;
constexpr auto kMetricsInterval = std::chrono::seconds(1);

std::atomic<bool> gRunning{true};

void handleSignal(int) {
    gRunning.store(false);
}

double clampDeviation(double value) {
    return std::clamp(value, -kDeviationClamp, kDeviationClamp);
}

std::string trim(const std::string& input) {
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

struct GeneratorSettings {
    double orders_per_second;
    int threads;
    double buy_only_seconds;
};

struct CliOptions {
    std::optional<double> orders_per_second;
    std::optional<Side> forced_side;
};

GeneratorSettings loadGeneratorSettings(const std::string& path,
                                        double defaultRate,
                                        int defaultThreads,
                                        double defaultBuyOnlySeconds) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {defaultRate, defaultThreads, defaultBuyOnlySeconds};
    }

    bool inGeneratorSection = false;
    std::string line;
    while (std::getline(file, line)) {
        std::string current = trim(line);
        if (current.empty() || current[0] == '#' || current[0] == ';') {
            continue;
        }
        if (current.front() == '[' && current.back() == ']') {
            const std::string section = current.substr(1, current.size() - 2);
            inGeneratorSection = (section == "generator");
            continue;
        }
        if (!inGeneratorSection) {
            continue;
        }
        const auto pos = current.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim(current.substr(0, pos));
        const std::string value = trim(current.substr(pos + 1));
        try {
            if (key == "orders_per_second") {
                defaultRate = std::max(0.0, std::stod(value));
            } else if (key == "threads") {
                defaultThreads = std::max(1, std::stoi(value));
            } else if (key == "buy_only_seconds") {
                defaultBuyOnlySeconds = std::max(0.0, std::stod(value));
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    return {defaultRate, defaultThreads, defaultBuyOnlySeconds};
}

std::optional<Side> parseSideValue(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (value == "BUY") return Side::BUY;
    if (value == "SELL") return Side::SELL;
    return std::nullopt;
}

CliOptions parseCommandLine(int argc, char** argv) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        const auto parseDouble = [&](const std::string& text, double& out) {
            try {
                out = std::stod(text);
                return true;
            } catch (const std::exception&) {
                return false;
            }
        };
        const auto parseSideOpt = [&](const std::string& text) {
            auto maybeSide = parseSideValue(text);
            if (!maybeSide) {
                throw std::invalid_argument("Invalid side '" + text + "', expected BUY or SELL");
            }
            opts.forced_side = maybeSide;
        };

        if (arg.rfind("--orders-per-second=", 0) == 0) {
            double value = 0.0;
            if (!parseDouble(arg.substr(21), value) || value < 0.0) {
                throw std::invalid_argument("Invalid value for --orders-per-second");
            }
            opts.orders_per_second = value;
        } else if (arg.rfind("--ops=", 0) == 0) {
            double value = 0.0;
            if (!parseDouble(arg.substr(6), value) || value < 0.0) {
                throw std::invalid_argument("Invalid value for --ops");
            }
            opts.orders_per_second = value;
        } else if (arg.rfind("--force-side=", 0) == 0) {
            parseSideOpt(arg.substr(13));
        } else if (arg.rfind("--side=", 0) == 0) {
            parseSideOpt(arg.substr(7));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: order_generator [--orders-per-second=N] [--force-side=BUY|SELL]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }
    return opts;
}

size_t serializeOrder(const ingress::WireOrder& order, std::array<char, 128>& buffer) {
    const auto side = ingress::toString(order.side);
    const auto type = ingress::toString(order.type);
    const int len = std::snprintf(
        buffer.data(),
        buffer.size(),
        "%llu,%u,%.*s,%llu,%u,%.*s,%u",
        static_cast<unsigned long long>(order.order_id),
        static_cast<unsigned>(order.instrument),
        static_cast<int>(side.size()),
        side.data(),
        static_cast<unsigned long long>(order.price),
        static_cast<unsigned>(order.quantity),
        static_cast<int>(type.size()),
        type.data(),
        static_cast<unsigned>(order.display)
    );
    return (len > 0) ? static_cast<size_t>(len) : 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions cli = parseCommandLine(argc, argv);
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        const AppConfig config = loadConfig(kConfigPath);
        const unsigned hwThreads = std::thread::hardware_concurrency();
        const int defaultThreads = static_cast<int>(hwThreads > 0 ? hwThreads : 1u);
        const auto settings = loadGeneratorSettings(kConfigPath, 200.0, defaultThreads, 0.0);
        const double configuredRate = cli.orders_per_second.value_or(settings.orders_per_second);
        const int workerCount = settings.threads;
        const auto buyOnlyDuration = std::chrono::duration<double>(settings.buy_only_seconds);
        const std::optional<Side> forcedSide = cli.forced_side;
        const double perThreadRate = (configuredRate <= 0.0)
            ? 0.0
            : (configuredRate / static_cast<double>(workerCount));
        const bool rateLimited = perThreadRate > 0.0;
        const auto minSpacing = rateLimited
            ? std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / perThreadRate))
            : Clock::duration::zero();

        auto createSocket = [&config] {
            SocketUtils::McastSocket socket;
            socket.init(config.mcast_ip, config.mcast_iface, config.mcast_port, false);
            return socket;
        };

        std::atomic<OrderId> nextId{1};
        std::atomic<uint64_t> totalSent{0};

        auto workerFn = [&](int workerId) {
            auto publisher = createSocket();
            auto nextSend = Clock::now();
            std::mt19937_64 rng(std::random_device{}() ^ (static_cast<uint64_t>(workerId) << 32U));
            std::normal_distribution<double> returns_dist(0.0, kSigma);
            std::uniform_int_distribution<int> qty_dist(kMinQty, kMaxQty);
            std::bernoulli_distribution side_dist(0.5);
            const bool warmupEnabled = !forcedSide.has_value() && settings.buy_only_seconds > 0.0;
            const Clock::time_point buyOnlyUntil = warmupEnabled
                ? Clock::now() + std::chrono::duration_cast<Clock::duration>(buyOnlyDuration)
                : Clock::time_point::min();

            while (gRunning.load(std::memory_order_relaxed)) {
                const double pct_move = clampDeviation(returns_dist(rng));
                const double priceValue = kClosingPrice * std::exp(pct_move);
                const Price price = static_cast<Price>(std::llround(std::max(priceValue, 1.0)));
                const Qty quantity = static_cast<Qty>(qty_dist(rng));
                Side side = Side::BUY;
                if (forcedSide.has_value()) {
                    side = *forcedSide;
                } else if (buyOnlyUntil != Clock::time_point::min() &&
                           Clock::now() < buyOnlyUntil) {
                    side = Side::BUY;
                } else {
                    side = side_dist(rng) ? Side::BUY : Side::SELL;
                }

                ingress::WireOrder order{
                    nextId.fetch_add(1, std::memory_order_relaxed),
                    kInstrumentToken,
                    side,
                    price,
                    quantity,
                    OrderType::LIMIT,
                    0
                };

                std::array<char, 128> payload{};
                const size_t payloadLen = serializeOrder(order, payload);

                if (rateLimited) {
                    auto now = Clock::now();
                    if (now < nextSend) {
                        std::this_thread::sleep_for(nextSend - now);
                        now = Clock::now();
                    }
                    nextSend = now + minSpacing;
                }

                publisher.send(payload.data(), payloadLen);
                publisher.sendAndRecv();
                totalSent.fetch_add(1, std::memory_order_relaxed);
            }
        };

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(workerCount));
        for (int i = 0; i < workerCount; ++i) {
            workers.emplace_back(workerFn, i);
        }

        const char* forcedSideText = forcedSide
            ? (*forcedSide == Side::BUY ? "BUY" : "SELL")
            : "mixed";
        LOG_INFO("Generator running for RELIANCE (token {}) | target {:.0f} orders/s | threads {} | {} mode | flow {} | buy-only phase {:.1f}s",
                 kInstrumentToken,
                 configuredRate,
                 workerCount,
                 rateLimited ? "rate-limited" : "unbounded",
                 forcedSideText,
                 settings.buy_only_seconds);

        std::thread metrics([&] {
            uint64_t lastCount = 0;
            while (gRunning.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(kMetricsInterval);
                const uint64_t now = totalSent.load(std::memory_order_relaxed);
                const uint64_t perSecond = now - lastCount;
#if !defined(ENABLE_INFO_LOGS)
                (void)perSecond;
#endif
                lastCount = now;
                LOG_INFO("Throughput: {} orders/s (total {})", perSecond, now);
            }
        });

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        if (metrics.joinable()) {
            metrics.join();
        }

        LOG_INFO("Generator stopped after sending {} orders", totalSent.load());
    } catch (const std::exception& ex) {
        LOG_ERROR("Generator error: {}", ex.what());
        return 1;
    }

    return 0;
}
