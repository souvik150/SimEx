//
// Created by souvik on 11/12/25.
//

#ifndef ORDERMATCHINGSYSTEM_MCAST_SOCKET_H
#define ORDERMATCHINGSYSTEM_MCAST_SOCKET_H
#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace SocketUtils {
    /// size of send and receive buffers in bytes.
    constexpr size_t McastBufferSize = 64 * 1024 * 1024;

    /// RAII + PImpl-based Multicast Socket wrapper.
    class McastSocket {
    public:
        explicit McastSocket();
        ~McastSocket();

        // Non-copyable, but movable (RAII safe)
        McastSocket(const McastSocket&) = delete;
        McastSocket& operator=(const McastSocket&) = delete;
        McastSocket(McastSocket&&) noexcept;
        McastSocket& operator=(McastSocket&&) noexcept;

        /// Initialize multicast socket to read from or publish to a stream.
        /// Does not join the multicast stream yet.
        auto init(const std::string& ip, const std::string& iface, int port, bool is_listening) -> int;

        /// Add / Join membership / subscription to a multicast stream.
        auto join(const std::string& ip) -> bool;

        /// Remove / Leave membership / subscription to a multicast stream.
        auto leave(const std::string& ip, int port) -> void;

        /// Publish outgoing data and read incoming data.
        auto sendAndRecv() noexcept -> bool;

        /// Copy data to send buffers - does not send them out yet.
        auto send(const void* data, size_t len) noexcept -> void;

        /// Register callback for when data is received.
        void setRecvCallback(std::function<void(McastSocket*)> cb) noexcept;

        void resetRecvSize() noexcept;

        /// Accessors (non-breaking)
        [[nodiscard]] int fd() const noexcept;
        [[nodiscard]] std::vector<char>& outboundBuffer() noexcept;
        [[nodiscard]] std::vector<char>& inboundBuffer() noexcept;
        [[nodiscard]] size_t recvSize() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

}

#endif //ORDERMATCHINGSYSTEM_MCAST_SOCKET_H