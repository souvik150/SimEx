#include "ingress/OrderDispatcher.h"

#include <chrono>
#include <cerrno>
#include <sys/epoll.h>
#include <string>
#include <thread>

#include "utils/LogMacros.h"

OrderDispatcher::OrderDispatcher(SocketUtils::McastSocket& socket, QueueMap queues)
    : socket_(socket),
      queues_(std::move(queues)) {}

void OrderDispatcher::run() {
    socket_.setRecvCallback([this](SocketUtils::McastSocket* sock) {
        const std::string payload(sock->inboundBuffer().data(), sock->recvSize());
        sock->resetRecvSize();
        handlePayload(payload);
    });

    const int fd = socket_.fd();
    const int epfd = ::epoll_create1(0);
    if (epfd == -1) {
        LOG_WARN("Failed to create epoll instance: {}", errno);
        return;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        LOG_WARN("Failed to add dispatcher fd to epoll: {}", errno);
        ::close(epfd);
        return;
    }

    while (running_) {
        epoll_event events[1];
        const int rc = ::epoll_wait(epfd, events, 1, 100);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_WARN("epoll_wait failed on dispatcher socket: {}", errno);
            break;
        }
        if (rc == 0) {
            continue;
        }
        if (events[0].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
            socket_.sendAndRecv();
        }
    }
    ::close(epfd);
}

void OrderDispatcher::stop() {
    running_ = false;
}

void OrderDispatcher::handlePayload(std::string_view payload) {
    if (payload.empty()) {
        return;
    }

    ingress::WireOrder order{};
    if (!ingress::parseWireOrder(payload, order)) {
        LOG_WARN("Failed to parse incoming payload '{}'", payload);
        return;
    }

    auto it = queues_.find(order.instrument);
    if (it == queues_.end()) {
        LOG_WARN("No queue registered for instrument {}", order.instrument);
        return;
    }

    auto* queue = it->second;
    std::size_t spins = 0;
    while (!queue->push(order)) {
        if (++spins % 1000 == 0) {
            std::this_thread::yield();
        }
    }
}
