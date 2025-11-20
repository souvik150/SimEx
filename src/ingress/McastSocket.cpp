#include "ingress/McastSocket.h"

#include <cstring>
#include <arpa/inet.h>
#include <unordered_set>
#include <utility>
#include <sys/socket.h>
#include <unistd.h>

#include "ingress/SocketUtils.h"
#include "utils/LogMacros.h"
#include "utils/Macros.h"
#include "utils/TimeUtils.h"

namespace SocketUtils {

class McastSocket::Impl {
public:
    Impl() {
        outbound_data_.resize(McastBufferSize);
        inbound_data_.resize(McastBufferSize);
    }

    ~Impl() {
        closeSocket();
    }

    void closeSocket() {
        if (socket_fd_ != -1) {
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
    }

    void trackJoinedGroup(const std::string& group) {
        joined_groups_.insert(group);
    }

    bool dropGroup(const std::string& group) {
        if (socket_fd_ == -1) {
            return false;
        }
        if (joined_groups_.find(group) == joined_groups_.end()) {
            return false;
        }

        const ip_mreq mreq{
            {inet_addr(group.c_str())},
            {htonl(INADDR_ANY)}
        };

        if (setsockopt(socket_fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
            LOG_WARN("Failed to drop multicast membership for {}", group);
            return false;
        }
        joined_groups_.erase(group);
        return true;
    }

    int socket_fd_ = -1;
    std::vector<char> outbound_data_;
    size_t next_send_valid_index_ = 0;
    std::vector<char> inbound_data_;
    size_t next_rcv_valid_index_ = 0;
    std::function<void(McastSocket*)> recv_callback_;
    std::string time_str_;
    std::unordered_set<std::string> joined_groups_;
    std::string iface_;
};

McastSocket::McastSocket()
    : impl_(std::make_unique<Impl>()) {}

McastSocket::~McastSocket() = default;
McastSocket::McastSocket(McastSocket&&) noexcept = default;
McastSocket& McastSocket::operator=(McastSocket&&) noexcept = default;

auto McastSocket::init(const std::string& ip,
                       const std::string& iface,
                       int port,
                       bool is_listening) -> int {
    const SocketCfg socket_cfg{ip, iface, port, true, is_listening, false};
    impl_->socket_fd_ = createSocket(socket_cfg);
    impl_->iface_ = iface;
    return impl_->socket_fd_;
}

bool McastSocket::join(const std::string& ip) {
    bool joined = false;
    auto attemptJoin = [&](in_addr addr, std::string_view label) {
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(ip.c_str());
        mreq.imr_interface = addr;
        if (setsockopt(impl_->socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
            LOG_WARN("Failed to join {} on {}: {}", ip, label, strerror(errno));
            return false;
        }
        LOG_INFO("Joined multicast group {} on {}", ip, label);
        return true;
    };

    in_addr iface_addr{};
    bool has_iface = false;
    if (!impl_->iface_.empty()) {
        const std::string iface_ip = SocketUtils::getIfaceIP(impl_->iface_);
        if (!iface_ip.empty()) {
            iface_addr.s_addr = inet_addr(iface_ip.c_str());
            has_iface = true;
        }
    }
    if (has_iface) {
        joined |= attemptJoin(iface_addr, impl_->iface_);
    }
    in_addr any_addr{};
    any_addr.s_addr = htonl(INADDR_ANY);
    joined |= attemptJoin(any_addr, "INADDR_ANY");

    if (joined) {
        impl_->trackJoinedGroup(ip);
    } else {
        LOG_ERROR("Failed to join multicast group {} on any interface", ip);
    }
    return joined;
}

auto McastSocket::leave(const std::string& ip, int) -> void {
    if (!impl_->dropGroup(ip)) {
        LOG_WARN("Requested to leave {}, but no membership was active", ip);
    }
}

auto McastSocket::sendAndRecv() noexcept -> bool {
    if (impl_->socket_fd_ == -1) {
        return false;
    }

    const ssize_t n_rcv = ::recv(
        impl_->socket_fd_,
        impl_->inbound_data_.data() + impl_->next_rcv_valid_index_,
        McastBufferSize - impl_->next_rcv_valid_index_,
        MSG_DONTWAIT
    );

    if (n_rcv > 0) {
        impl_->next_rcv_valid_index_ += static_cast<size_t>(n_rcv);
        LOG_DEBUG("read socket:{} len:{} time:{}",
                  impl_->socket_fd_,
                  impl_->next_rcv_valid_index_,
                  Common::getCurrentTimeStr(&impl_->time_str_));

        if (impl_->recv_callback_) {
            impl_->recv_callback_(this);
        }
    }

    if (impl_->next_send_valid_index_ > 0) {
        const ssize_t n = ::send(
            impl_->socket_fd_,
            impl_->outbound_data_.data(),
            impl_->next_send_valid_index_,
            MSG_DONTWAIT | MSG_NOSIGNAL
        );

        LOG_DEBUG("send socket:{} len:{} time:{}",
                  impl_->socket_fd_,
                  n,
                  Common::getCurrentTimeStr(&impl_->time_str_));
    }

    impl_->next_send_valid_index_ = 0;
    return (n_rcv > 0);
}

auto McastSocket::send(const void* data, size_t len) noexcept -> void {
    ASSERT(impl_->next_send_valid_index_ + len <= McastBufferSize,
           "Mcast socket buffer filled up and sendAndRecv() not called.");
    std::memcpy(
        impl_->outbound_data_.data() + impl_->next_send_valid_index_,
        data,
        len
    );
    impl_->next_send_valid_index_ += len;
}

void McastSocket::setRecvCallback(std::function<void(McastSocket*)> cb) noexcept {
    impl_->recv_callback_ = std::move(cb);
}

size_t McastSocket::recvSize() const noexcept {
    return impl_->next_rcv_valid_index_;
}

void McastSocket::resetRecvSize() noexcept {
    impl_->next_rcv_valid_index_ = 0;
}

int McastSocket::fd() const noexcept {
    return impl_->socket_fd_;
}

std::vector<char>& McastSocket::outboundBuffer() noexcept {
    return impl_->outbound_data_;
}

std::vector<char>& McastSocket::inboundBuffer() noexcept {
    return impl_->inbound_data_;
}

} // namespace SocketUtils
