#pragma once

// WLED wire formats: UDP sync notifier, node-info (sysInfo), realtime
// (WARLS/DRGB/DRGBW/DNRGB/DNRGBW) and TPM2.NET.  Pure functions, no I/O.

#include <cstdint>
#include <cstring>

namespace esphome {
namespace wled_emu {
namespace proto {

static constexpr size_t NOTIFIER_LEN = 41;
static constexpr size_t SYSINFO_LEN = 44;
// Reported as WLED 16.0.1 (current release): apps compare version / build ids.
static constexpr const char *BUILD_VER = "16.0.1";
static constexpr uint32_t BUILD_VID = 2606300;
static constexpr uint16_t TPM2_ACK_PORT = 65442;

// Node-type ids as reported by WLED v16 /json/nodes.
static constexpr uint8_t NODE_TYPE_ESP32 = 32;
static constexpr uint8_t NODE_TYPE_ESP8266 = 210;

/// Build a 41-byte notifier (proto 0). compatibilityVersion is pinned to 10:
/// v11+ advertises a per-segment block, and segCount=0 with v12 makes peers
/// that have "Receive Segment bounds" enabled trim their segment list to
/// zero. v10 receivers parse master-level fields (bri, fx, palette,
/// timebase, sync groups, CCT) and skip segments.
inline void build_notifier(uint8_t pkt[NOTIFIER_LEN], bool on, uint8_t bri, uint8_t r, uint8_t g, uint8_t b,
                           uint8_t fxmode, uint8_t groups, bool follow_up) {
  memset(pkt, 0, NOTIFIER_LEN);
  pkt[0] = 0;              // proto: notifier
  pkt[1] = 1;              // callMode: DIRECT_CHANGE
  pkt[2] = on ? bri : 0;   // master brightness
  pkt[3] = r;
  pkt[4] = g;
  pkt[5] = b;
  pkt[8] = fxmode;
  pkt[9] = 128;            // effect speed
  pkt[11] = 10;            // compatibility version
  pkt[16] = 128;           // effect intensity
  pkt[24] = follow_up ? 1 : 0;
  pkt[36] = groups;
  pkt[37] = 255;           // CCT: no valid value
  pkt[39] = 0;
  pkt[40] = 36;            // palette
}

struct Notifier {
  uint8_t bri;
  uint8_t r, g, b;
  uint8_t fxmode;
  uint8_t groups;
};

/// Parse a notifier. Packets older than v9 carry no group byte → group 1.
inline bool parse_notifier(const uint8_t *d, int n, Notifier &out) {
  if (n < 12 || d[0] != 0)
    return false;
  out.bri = d[2];
  out.r = d[3];
  out.g = d[4];
  out.b = d[5];
  out.fxmode = d[8];
  out.groups = (d[11] >= 9 && n >= 37) ? d[36] : 0x01;
  return true;
}

/// Build the 44-byte node-info broadcast (WLED sendSysInfoUDP layout):
/// 0=255, 1=1, 2..5=IP, 6..37=name, 38=node type (|0x80 if on),
/// 39=unit id (last IP byte), 40..43=build vid LE.
inline void build_sysinfo(uint8_t pkt[SYSINFO_LEN], const uint8_t ip[4], const char *name, uint8_t node_type,
                          bool on) {
  memset(pkt, 0, SYSINFO_LEN);
  pkt[0] = 255;
  pkt[1] = 1;
  memcpy(pkt + 2, ip, 4);
  size_t nl = strlen(name);
  if (nl > 32)
    nl = 32;
  memcpy(pkt + 6, name, nl);
  pkt[38] = node_type | (on ? 0x80 : 0x00);
  pkt[39] = ip[3];
  pkt[40] = (BUILD_VID >> 0) & 0xFF;
  pkt[41] = (BUILD_VID >> 8) & 0xFF;
  pkt[42] = (BUILD_VID >> 16) & 0xFF;
  pkt[43] = (BUILD_VID >> 24) & 0xFF;
}

struct SysInfo {
  uint8_t ip[4];
  char name[33];
  uint8_t type;
  uint32_t vid;
};

inline bool parse_sysinfo(const uint8_t *d, int n, SysInfo &out) {
  if (n < 40 || d[0] != 255 || d[1] != 1)
    return false;
  memcpy(out.ip, d + 2, 4);
  memcpy(out.name, d + 6, 32);
  out.name[32] = 0;
  out.type = d[38];
  out.vid = n >= 44 ? ((uint32_t) d[40] | ((uint32_t) d[41] << 8) | ((uint32_t) d[42] << 16) |
                       ((uint32_t) d[43] << 24))
                    : 0;
  return true;
}

struct Realtime {
  uint8_t timeout_s;  // 0 = leave realtime mode
  uint8_t r, g, b, w;
};

/// First-LED colour from WARLS(1)/DRGB(2)/DRGBW(3)/DNRGB(4)/DNRGBW(5).
inline bool parse_realtime(const uint8_t *d, int n, Realtime &out) {
  if (n < 2 || d[0] < 1 || d[0] > 5)
    return false;
  out.timeout_s = d[1];
  out.r = out.g = out.b = out.w = 0;
  switch (d[0]) {
    case 1:
      if (n >= 6 && d[2] == 0) { out.r = d[3]; out.g = d[4]; out.b = d[5]; }
      break;
    case 2:
      if (n >= 5) { out.r = d[2]; out.g = d[3]; out.b = d[4]; }
      break;
    case 3:
      if (n >= 6) { out.r = d[2]; out.g = d[3]; out.b = d[4]; out.w = d[5]; }
      break;
    case 4:
      if (n >= 7) { out.r = d[4]; out.g = d[5]; out.b = d[6]; }
      break;
    case 5:
      if (n >= 8) { out.r = d[4]; out.g = d[5]; out.b = d[6]; out.w = d[7]; }
      break;
  }
  return true;
}

}  // namespace proto
}  // namespace wled_emu
}  // namespace esphome
