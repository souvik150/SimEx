//
// Created by souvik on 11/12/25.
//

#ifndef ORDERMATCHINGSYSTEM_SOCKET_UTILS_H
#define ORDERMATCHINGSYSTEM_SOCKET_UTILS_H

#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>

#include "utils/Macros.h"
#include "utils/LogMacros.h"
#include "utils/TimeUtils.h"

namespace SocketUtils {
  class SocketCfg {
  public:
    std::string ip_;
    std::string iface_;
    int port_ = -1;
    bool is_udp_ = false;
    bool is_listening_ = false;
    bool needs_so_timestamp_ =  false;

    [[nodiscard]] std::string toString() const {
      std::ostringstream ss;
      ss << "SocketCfg[ip:" << ip_
         << " iface:" << iface_
         << " port:" << port_
         << " is_udp:" << std::boolalpha << is_udp_
         << " is_listening:" << is_listening_
         << " needs_SO_timestamp:" << needs_so_timestamp_
         << "]";
      const std::string desc = ss.str();
      LOG_INFO("{}", desc);
      return desc;
    }
  };

  constexpr int MaxTCPServerBacklog = 1024;

  /// Convert interface name "eth0" to ip "123.123.123.123".
  inline auto getIfaceIP(const std::string &iface) -> std::string {
    // NI_MAXHOST -> it defines the maximum length of a host name/numeric address string.
    char buf[NI_MAXHOST] = {'\0'};
    ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) != -1) {
      for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        // AF_INET -> it tells to look only for IPv4
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name) {
          // NI_NUMERICHOST -> it tells to return the numeric form and not try dns resolution
          getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
          break;
        }
      }

      // needed as get would dynamically allocate memory
      freeifaddrs(ifaddr);
    }

    return buf;
  }

  /// sockets will not block on read, but instead return immediately if data is not available.
  inline auto setNonBlocking(int fd) -> bool {
    const auto flags = fcntl(fd, F_GETFL, 0);
    if (flags & O_NONBLOCK)
      return true;
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
  }

  /// disable nagle's algorithm to avoid the wait on previous packets
  inline auto disableNagle(int fd) -> bool {
    int one = 1;
    return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
  }

  /// allow software receive timestamps on incoming packets.
  inline auto setSOTimestamp(int fd) -> bool {
    int one = 1;
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
  }

  /// Add / Join membership / subscription to the multicast stream specified and on the interface specified.
  inline auto join(int fd, const std::string &ip) -> bool {
    const ip_mreq mreq{
      {inet_addr(ip.c_str())},
      {htonl(INADDR_ANY)}
    };

    return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
  }

  /// Create a TCP / UDP socket to either connect to or listen for data on or listen for connections on the specified interface and IP:port information.
  [[nodiscard]] inline auto createSocket(const SocketCfg& socket_cfg) -> int {
    const auto ip = socket_cfg.ip_.empty() ? getIfaceIP(socket_cfg.iface_) : socket_cfg.ip_;
    const auto iface_ip = socket_cfg.iface_.empty() ? std::string{} : getIfaceIP(socket_cfg.iface_);
    LOG_INFO("Creating socket with cfg: {}", socket_cfg.toString());

    // AI_NUMERICHOST -> The host string is a numeric address (e.g. 192.168.0.5, 239.192.1.1), not a hostname. Don’t do DNS resolution.
    // AI_NUMERICSERV -> The service argument (port) is numeric (e.g. "5001"), not a service name (like "http").
    const int input_flags = (socket_cfg.is_listening_ ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);
    const addrinfo hints{
      input_flags,
      AF_INET,
      socket_cfg.is_udp_ ? SOCK_DGRAM : SOCK_STREAM,
      socket_cfg.is_udp_ ? IPPROTO_UDP : IPPROTO_TCP,
      0,
      0,
      nullptr,
      nullptr
    };

    addrinfo *result = nullptr;
    const auto rc = getaddrinfo(ip.c_str(), std::to_string(socket_cfg.port_).c_str(), &hints, &result);
    ASSERT(!rc, "getaddrinfo() failed. error:" + std::string(gai_strerror(rc)) + "errno:" + strerror(errno));

    int socket_fd = -1;
    int one = 1;
    for (addrinfo *rp = result; rp; rp = rp->ai_next) {
      ASSERT((socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1, "socket() failed. errno:" + std::string(strerror(errno)));

      ASSERT(setNonBlocking(socket_fd), "setNonBlocking() failed. errno:" + std::string(strerror(errno)));

      if (!socket_cfg.is_udp_) {
        ASSERT(disableNagle(socket_fd), "disableNagle() failed. errno:" + std::string(strerror(errno)));
      }

      if (!socket_cfg.is_listening_) {
        ASSERT(connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1, "connect() failed. errno:" + std::string(strerror(errno)));
      }

      if (socket_cfg.is_listening_) {
        ASSERT(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one)) == 0, "setsockopt() SO_REUSEADDR failed. errno:" + std::string(strerror(errno)));
      }

      if (socket_cfg.is_listening_) {
        const sockaddr_in addr{AF_INET, htons(socket_cfg.port_), {htonl(INADDR_ANY)}, {}};
        ASSERT(bind(socket_fd, socket_cfg.is_udp_ ? reinterpret_cast<const struct sockaddr *>(&addr) : rp->ai_addr, sizeof(addr)) == 0, "bind() failed. errno:%" + std::string(strerror(errno)));
      }

      if (!socket_cfg.is_udp_ && socket_cfg.is_listening_) { // listen for incoming TCP connections.
        ASSERT(listen(socket_fd, MaxTCPServerBacklog) == 0, "listen() failed. errno:" + std::string(strerror(errno)));
      }

      if (socket_cfg.is_udp_ && !socket_cfg.is_listening_ && !iface_ip.empty()) {
        in_addr local_if{};
        local_if.s_addr = inet_addr(iface_ip.c_str());
        ASSERT(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_IF, &local_if, sizeof(local_if)) == 0,
               "setsockopt() IP_MULTICAST_IF failed. errno:" + std::string(strerror(errno)));
        unsigned char loop = 1;
        ASSERT(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) == 0,
               "setsockopt() IP_MULTICAST_LOOP failed. errno:" + std::string(strerror(errno)));
      }

      if (socket_cfg.needs_so_timestamp_) { // enable software receive timestamps.
        ASSERT(setSOTimestamp(socket_fd), "setSOTimestamp() failed. errno:" + std::string(strerror(errno)));
      }
    }

    return socket_fd;
  }
}
#endif //ORDERMATCHINGSYSTEM_SOCKET_UTILS_H
