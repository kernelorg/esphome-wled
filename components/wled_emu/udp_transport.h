#pragma once

// Minimal non-blocking UDP endpoint with two backends:
//   * ESP32 / LibreTiny / host — ESPHome's socket wrapper (BSD or lwIP
//     sockets).  Broadcast-capable, rebindable at runtime, self-healing
//     on send errors.
//   * ESP8266 / RP2040 — Arduino WiFiUDP (the Arduino ESP8266 core has
//     no BSD socket API; ESPHome selects USE_SOCKET_IMPL_LWIP_TCP there).

#include "esphome/core/defines.h"
#include <cstddef>
#include <cstdint>
#include <memory>

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#define WLED_EMU_USE_SOCKETS
#elif defined(USE_SOCKET_IMPL_LWIP_TCP)
#include <WiFiUdp.h>
#define WLED_EMU_USE_WIFIUDP
#else
#error "wled_emu: unsupported socket implementation"
#endif

namespace esphome {
namespace wled_emu {

class UdpEndpoint {
 public:
  /// Bind to `port` (0 = ephemeral, send-only).  Re-binding to another port
  /// closes the previous socket.  Returns true when the socket is ready.
  bool open(uint16_t port);
  void close();
  bool is_open() const;
  uint16_t port() const { return this->port_; }

  /// Non-blocking receive. Returns the number of bytes copied (0 = nothing
  /// pending, <0 = error). `src_ip` receives the sender IPv4 as a.b.c.d bytes.
  int recv(uint8_t *buf, size_t cap, uint8_t src_ip[4]);

  /// Send to 255.255.255.255:port.
  bool broadcast(uint16_t port, const uint8_t *data, size_t len);
  /// Send to a unicast IPv4 address (a.b.c.d bytes).
  bool send_to(const uint8_t ip[4], uint16_t port, const uint8_t *data, size_t len);

 protected:
  bool send_raw_(uint32_t ip_be, uint16_t port, const uint8_t *data, size_t len);

  uint16_t port_{0};
  bool want_open_{false};
#ifdef WLED_EMU_USE_SOCKETS
  std::unique_ptr<socket::Socket> sock_;
#endif
#ifdef WLED_EMU_USE_WIFIUDP
  WiFiUDP udp_;
  bool open_{false};
#endif
};

}  // namespace wled_emu
}  // namespace esphome
