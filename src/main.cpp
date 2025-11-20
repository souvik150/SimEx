#include <atomic>
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
#include "utils/Affinity.h"
#include "utils/Config.h"
#include "utils/Logger.h"
#include "utils/LogMacros.h"

namespace {
#ifdef PROJECT_ROOT
constexpr const char* kConfigPath = PROJECT_ROOT "/config/app.ini";
#else
constexpr const char* kConfigPath = "config/app.ini";
#endif

using WireOrder = ingress::WireOrder;
using Queue = boost::lockfree::spsc_queue<WireOrder>;
constexpr std::size_t kQueueCapacity = 10240;
}  // namespace

int main() {
    try {
        const AppConfig config = loadConfig(kConfigPath);
        logging::LoggerOptions logger_opts;
        logger_opts.queue_size = config.logging.queue_size;
        logger_opts.worker_threads = config.logging.worker_threads;
        logger_opts.affinity = config.affinity.logging_cores;
        logging::configureLogger(logger_opts);
        const std::vector<InstrumentToken> instruments = {26000, 35000};

        std::unordered_map<InstrumentToken, std::unique_ptr<Queue>> queue_storage;
        OrderDispatcher::QueueMap dispatcher_queues;
        std::unordered_map<InstrumentToken, std::unique_ptr<OrderBook>> books;

        SnapshotConfig snapshot_cfg;
        snapshot_cfg.shm_prefix = config.snapshot.shm_prefix;
        snapshot_cfg.interval = std::chrono::milliseconds(config.snapshot.interval_ms);
        snapshot_cfg.max_levels = config.snapshot.levels;
        SnapshotPublisher publisher(snapshot_cfg, instruments);

        const auto& engineCores = config.affinity.engine_cores;
        size_t nextEngineCore = 0;

        for (const auto token : instruments) {
            auto queue = std::make_unique<Queue>(kQueueCapacity);
            dispatcher_queues[token] = queue.get();
            queue_storage[token] = std::move(queue);
            books[token] = std::make_unique<OrderBook>(config.use_std_map);
            books[token]->setInstrumentToken(token);
            if (!engineCores.empty()) {
                const int tradeCore = engineCores[nextEngineCore % engineCores.size()];
                ++nextEngineCore;
                books[token]->bindTradeThreadToCores(std::vector<int>{tradeCore});
            }
        }

        std::vector<std::thread> workers;
        workers.reserve(instruments.size());

        for (std::size_t idx = 0; idx < instruments.size(); ++idx) {
            const InstrumentToken token = instruments[idx];
            Queue* queue = dispatcher_queues[token];
            OrderBook* book = books[token].get();

            int workerCore = -1;
            if (!engineCores.empty()) {
                workerCore = engineCores[nextEngineCore % engineCores.size()];
                ++nextEngineCore;
            }

            workers.emplace_back([queue, book, token, &publisher, workerCore] {
                if (workerCore >= 0) {
                    cpu::setCurrentThreadAffinity(std::vector<int>{workerCore});
                }
                while (true) {
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
                }
            });
            if (workerCore >= 0) {
                cpu::setThreadAffinity(workers.back(), workerCore);
            }
        }

        SocketUtils::McastSocket socket;
        socket.init(config.mcast_ip, config.mcast_iface, config.mcast_port, true);
        socket.join(config.mcast_ip);

        OrderDispatcher dispatcher(socket, dispatcher_queues);
        std::thread dispatcher_thread([&dispatcher] {
            dispatcher.run();
        });

        LOG_INFO("Engine ready on {}:{} via iface {} (orderbook backend: {})",
                 config.mcast_ip,
                 config.mcast_port,
                 config.mcast_iface,
                 config.use_std_map ? "std::pmr::map" : "RingBuffer");

        std::atomic<bool> running{true};

        dispatcher_thread.join();
        running.store(false, std::memory_order_relaxed);
        for (auto& worker : workers) {
            worker.join();
        }
    } catch (const std::exception& ex) {
        LOG_ERROR("Engine crashed: {}", ex.what());
        return 1;
    }

    return 0;
}
