#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "boost/lockfree/spsc_queue.hpp"
#include "ingress/McastSocket.h"
#include "ingress/WireOrder.h"

class OrderDispatcher {
public:
    using Queue = boost::lockfree::spsc_queue<ingress::WireOrder>;
    using QueueMap = std::unordered_map<InstrumentToken, Queue*>;

    OrderDispatcher(SocketUtils::McastSocket& socket, QueueMap queues);

    void run();
    void stop();

private:
    void handlePayload(std::string_view payload);

    SocketUtils::McastSocket& socket_;
    QueueMap queues_;
    std::atomic<bool> running_{true};
    std::unordered_set<InstrumentToken> seen_instruments_;
};
