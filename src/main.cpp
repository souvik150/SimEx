//
// Created by souvik on 11/8/25.
//

#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

#include "boost/lockfree/spsc_queue.hpp"
#include "core/OrderBook.h"
#include "core/OrderBuilder.h"
#include "ingress/McastSocket.h"
#include "ingress/OrderDispatcher.h"
#include "ingress/WireOrder.h"
#include "snapshot/SnapshotPublisher.h"
#include "utils/LatencyStats.h"
#include "utils/Config.h"
#include "utils/LogMacros.h"
#include "utils/TimeUtils.h"

namespace {
#ifdef PROJECT_ROOT
constexpr const char* kConfigPath = PROJECT_ROOT "/config/app.ini";
#else
constexpr const char* kConfigPath = "config/app.ini";
#endif

using WireOrder = ingress::WireOrder;
using Queue = boost::lockfree::spsc_queue<WireOrder>;
constexpr std::size_t kQueueCapacity = 10240;
}

int main() {
    try {
        // Instruments we want to handle. Each will get its own queue + worker thread.
        const AppConfig config = loadConfig(kConfigPath);
        const std::vector<InstrumentToken> instruments = {26000, 35000};

        std::unordered_map<InstrumentToken, std::unique_ptr<Queue>> queue_storage;
        OrderDispatcher::QueueMap dispatcher_queues;
        std::unordered_map<InstrumentToken, std::unique_ptr<OrderBook>> books;

        SnapshotConfig snapshot_cfg;
        snapshot_cfg.shm_prefix = config.snapshot.shm_prefix;
        snapshot_cfg.interval = std::chrono::milliseconds(config.snapshot.interval_ms);
        snapshot_cfg.max_levels = config.snapshot.levels;
        SnapshotPublisher publisher(snapshot_cfg, instruments);

        for (const auto token : instruments) {
            auto queue = std::make_unique<Queue>(kQueueCapacity);
            dispatcher_queues[token] = queue.get();
            queue_storage[token] = std::move(queue);
            books[token] = std::make_unique<OrderBook>(config.use_std_map);
            books[token]->setInstrumentToken(token);
        }

        std::vector<std::thread> workers;
        std::vector<LatencyStats> stats(instruments.size());
        workers.reserve(instruments.size());

        for (std::size_t idx = 0; idx < instruments.size(); ++idx) {
            const auto token = instruments[idx];
            Queue* queue = dispatcher_queues[token];
            OrderBook* book = books[token].get();
            LatencyStats* stat = &stats[idx];

            workers.emplace_back([queue, book, token, stat, &publisher] {
                while (true) {
                    const auto start = std::chrono::high_resolution_clock::now();
                    WireOrder inbound;
                    std::size_t spins = 0;
                    while (!queue->pop(inbound)) {
                        if (++spins % 1000 == 0) {
                            std::this_thread::yield();
                        }
                    }
                    OrderBuilder builder;
                    builder.setOrderId(inbound.order_id)
                        .setInstrumentToken(inbound.instrument)
                        .setSide(inbound.side)
                        .setPrice(inbound.price)
                        .setQuantity(inbound.quantity)
                        .setOrderType(inbound.type)
                        .setTimestamp(std::chrono::high_resolution_clock::now());

                    if (inbound.display > 0) {
                        builder.setDisplayQuantity(inbound.display);
                    }

                    auto order = builder.build();
                    book->addOrder(std::move(order));
                    publisher.maybePublish(token, *book);
                    const auto end = std::chrono::high_resolution_clock::now();
                    stat->observe(static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
                }
            });
        }

        SocketUtils::McastSocket socket;
        socket.init(config.mcast_ip, config.mcast_iface, config.mcast_port, true);
        socket.join(config.mcast_ip);

        OrderDispatcher dispatcher(socket, dispatcher_queues);
        std::thread dispatcher_thread   ([&dispatcher] {
            dispatcher.run();
        });

        LOG_INFO("Engine ready on {}:{} via iface {} (orderbook backend: {})",
                 config.mcast_ip,
                 config.mcast_port,
                 config.mcast_iface,
                 config.use_std_map ? "std::pmr::map" : "RBTree");

        std::atomic<bool> running{true};
        std::thread metrics_thread([&] {
            while (running.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                for (std::size_t i = 0; i < stats.size(); ++i) {
                    const auto count = stats[i].count();
                    if (count == 0) continue;
                    LOG_INFO("Token {} latency ns: avg {:.0f} min {} max {} samples {}",
                             instruments[i],
                             stats[i].average(),
                             stats[i].min(),
                             stats[i].max(),
                             count);
                    stats[i].reset();
                }
            }
        });

        dispatcher_thread.join();
        running.store(false);
        if (metrics_thread.joinable()) {
            metrics_thread.join();
        }
        for (auto& worker : workers) {
            worker.join();
        }
    } catch (const std::exception& ex) {
        LOG_ERROR("Engine crashed: {}", ex.what());
        return 1;
    }

    return 0;
}
