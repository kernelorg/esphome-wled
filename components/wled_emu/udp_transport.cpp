#include "udp_transport.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>

namespace esphome {
namespace wled_emu {

static const char *const TAG = "wled_emu.udp";

#ifdef WLED_EMU_USE_SOCKETS

bool UdpEndpoint::open(uint16_t port) {
  if (this->sock_ != nullptr && this->port_ == port)
    return true;
  this->close();
  this->want_open_ = true;
  this->port_ = port;
  this->sock_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (this->sock_ == nullptr) {
    ESP_LOGW(TAG, "socket() failed: errno %d", errno);
    return false;
  }
  int one = 1;
  this->sock_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  this->sock_->setsockopt(SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
  if (this->sock_->setblocking(false) < 0) {
    ESP_LOGW(TAG, "setblocking(false) failed: errno %d", errno);
    this->sock_.reset();
    return false;
  }
  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ESPHOME_INADDR_ANY;
  addr.sin_port = htons(port);
  if (this->sock_->bind(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
    // A transient bind failure (port briefly busy, lwIP slot pressure)
    // must not latch: drop the socket so the next open() retries.
    ESP_LOGW(TAG, "bind(%u) failed: errno %d", port, errno);
    this->sock_.reset();
    return false;
  }
  ESP_LOGD(TAG, "bound UDP port %u", port);
  return true;
}

void UdpEndpoint::close() {
  this->sock_.reset();
  this->want_open_ = false;
}

bool UdpEndpoint::is_open() const { return this->sock_ != nullptr; }

int UdpEndpoint::recv(uint8_t *buf, size_t cap, uint8_t src_ip[4]) {
  if (this->sock_ == nullptr)
    return -1;
  struct sockaddr_in from {};
  socklen_t from_len = sizeof(from);
  ssize_t n = this->sock_->recvfrom(buf, cap, reinterpret_cast<struct sockaddr *>(&from), &from_len);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 0;
    return -1;
  }
  if (src_ip != nullptr) {
    uint32_t ip = from.sin_addr.s_addr;  // network byte order
    memcpy(src_ip, &ip, 4);
  }
  return (int) n;
}

bool UdpEndpoint::send_raw_(uint32_t ip_be, uint16_t port, const uint8_t *data, size_t len) {
  if (this->sock_ == nullptr && !this->open(this->port_))
    return false;
  struct sockaddr_in to {};
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = ip_be;
  to.sin_port = htons(port);
  ssize_t n = this->sock_->sendto(data, len, 0, reinterpret_cast<struct sockaddr *>(&to), sizeof(to));
  if (n < 0) {
    // Self-heal: a socket that errored (e.g. after a WiFi reconnect) is
    // recreated on the next send.
    ESP_LOGW(TAG, "sendto(%u) failed: errno %d — socket reset", port, errno);
    uint16_t p = this->port_;
    this->sock_.reset();
    this->port_ = p;
    return false;
  }
  return true;
}

bool UdpEndpoint::broadcast(uint16_t port, const uint8_t *data, size_t len) {
  return this->send_raw_(0xFFFFFFFFu, port, data, len);
}

bool UdpEndpoint::send_to(const uint8_t ip[4], uint16_t port, const uint8_t *data, size_t len) {
  uint32_t ip_be;
  memcpy(&ip_be, ip, 4);
  return this->send_raw_(ip_be, port, data, len);
}

#endif  // WLED_EMU_USE_SOCKETS

#ifdef WLED_EMU_USE_WIFIUDP

bool UdpEndpoint::open(uint16_t port) {
  if (this->open_ && this->port_ == port)
    return true;
  this->close();
  this->want_open_ = true;
  this->port_ = port;
  this->open_ = this->udp_.begin(port) != 0;
  if (!this->open_)
    ESP_LOGW(TAG, "WiFiUDP.begin(%u) failed", port);
  return this->open_;
}

void UdpEndpoint::close() {
  if (this->open_)
    this->udp_.stop();
  this->open_ = false;
  this->want_open_ = false;
}

bool UdpEndpoint::is_open() const { return this->open_; }

int UdpEndpoint::recv(uint8_t *buf, size_t cap, uint8_t src_ip[4]) {
  if (!this->open_)
    return -1;
  int len = this->udp_.parsePacket();
  if (len <= 0)
    return 0;
  int n = this->udp_.read(buf, cap);
  if ((size_t) len > cap)
    this->udp_.flush();  // drop the oversize remainder
  if (src_ip != nullptr) {
    IPAddress r = this->udp_.remoteIP();
    for (int i = 0; i < 4; i++)
      src_ip[i] = r[i];
  }
  return n;
}

bool UdpEndpoint::send_raw_(uint32_t ip_be, uint16_t port, const uint8_t *data, size_t len) {
  if (!this->open_ && !this->open(this->port_))
    return false;
  const uint8_t *b = reinterpret_cast<const uint8_t *>(&ip_be);
  if (this->udp_.beginPacket(IPAddress(b[0], b[1], b[2], b[3]), port) == 0)
    return false;
  this->udp_.write(data, len);
  return this->udp_.endPacket() != 0;
}

bool UdpEndpoint::broadcast(uint16_t port, const uint8_t *data, size_t len) {
  return this->send_raw_(0xFFFFFFFFu, port, data, len);
}

bool UdpEndpoint::send_to(const uint8_t ip[4], uint16_t port, const uint8_t *data, size_t len) {
  uint32_t ip_be;
  memcpy(&ip_be, ip, 4);
  return this->send_raw_(ip_be, port, data, len);
}

#endif  // WLED_EMU_USE_WIFIUDP

}  // namespace wled_emu
}  // namespace esphome
