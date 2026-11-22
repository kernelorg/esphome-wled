#include "wled_emu.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <cstdio>

namespace esphome {
namespace wled_emu {

static const char *const TAG = "wled_emu";

static constexpr uint32_t ECHO_COOLDOWN_MS = 500;
static constexpr uint32_t CHANGE_CHECK_MS = 250;
static constexpr uint32_t FOLLOW_UP_MS = 250;
static constexpr uint32_t TPM2_HOLD_MS = 2500;

// ── setup / loop ───────────────────────────────────────────────────────

void WledEmu::setup() {
  if (this->node_type_ == 0) {
#ifdef USE_ESP8266
    this->node_type_ = proto::NODE_TYPE_ESP8266;
#else
    this->node_type_ = proto::NODE_TYPE_ESP32;
#endif
  }
}

void WledEmu::dump_config() {
  ESP_LOGCONFIG(TAG,
                "WLED emulation:\n"
                "  Sync port: %u%s\n"
                "  Info port: %u (discovery %s)\n"
                "  Node type: %u\n"
                "  Groups send/recv: 0x%02X / 0x%02X",
                this->port_,
#ifdef USE_LIGHT
                this->light_ != nullptr ? " (listening)" : " (send only)",
#else
                " (send only)",
#endif
                this->info_port_, ONOFF(this->discovery_), this->node_type_, this->send_groups_,
                this->recv_groups_);
}

bool WledEmu::local_ip4(uint8_t out[4]) {
  if (!network::is_connected())
    return false;
  for (auto &ip : network::get_ip_addresses()) {
    if (!ip.is_set() || !ip.is_ip4())
      continue;
    char buf[network::IP_ADDRESS_BUFFER_SIZE];
    ip.str_to(buf);
    unsigned a, b, c, d;
    if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
      out[0] = a;
      out[1] = b;
      out[2] = c;
      out[3] = d;
      return true;
    }
  }
  return false;
}

void WledEmu::ensure_sockets_() {
  // Ports are runtime-settable; open() is a no-op when already bound to
  // the requested port, rebinds otherwise. A failed bind retries next loop.
  if (!this->tx_.is_open())
    this->tx_.open(0);
  if (this->discovery_)
    this->info_.open(this->info_port_);
#ifdef USE_LIGHT
  if (this->light_ != nullptr)
    this->sync_.open(this->port_);
#endif
}

void WledEmu::loop() {
  if (!network::is_connected())
    return;
  this->ensure_sockets_();
  uint32_t now = millis();
  if (this->http_loop_)
    this->http_loop_();

  if (this->discovery_) {
    this->drain_info_port_();
    if (now - this->last_age_ms_ >= 1000) {
      this->last_age_ms_ = now;
      this->age_nodes_();
    }
  }

  if (this->sync_enable_ && this->sysinfo_interval_ > 0 && now - this->last_sysinfo_ms_ >= this->sysinfo_interval_) {
    this->last_sysinfo_ms_ = now;
    this->send_sysinfo();
  }

#ifdef USE_LIGHT
  if (this->light_ == nullptr)
    return;
  this->drain_sync_port_();
  if (this->rt_until_ms_ != 0 && (int32_t) (millis() - this->rt_until_ms_) >= 0)
    this->leave_realtime_();
  if (this->follow_up_at_ != 0 && (int32_t) (millis() - this->follow_up_at_) >= 0) {
    this->follow_up_at_ = 0;
    if (this->sync_enable_ && this->sync_send_) {
      bool on;
      uint8_t bri, r, g, b, fx;
      this->current_light_(on, bri, r, g, b, fx);
      uint8_t pkt[proto::NOTIFIER_LEN];
      proto::build_notifier(pkt, on, bri, r, g, b, fx, this->send_groups_, true);
      this->broadcast_(this->port_, pkt, sizeof(pkt));
    }
  }
  if (now - this->last_change_check_ms_ >= CHANGE_CHECK_MS) {
    this->last_change_check_ms_ = now;
    this->check_send_on_change_();
  }
#endif
}

// ── effect table ───────────────────────────────────────────────────────

uint8_t WledEmu::fxmode_for_effect(const std::string &effect) const {
  for (const auto &m : this->effects_) {
    if (!m.wled_ids.empty() && str_equals_case_insensitive(effect, m.effect))
      return m.wled_ids[0];
  }
  return 0;
}

const char *WledEmu::effect_for_fxmode(uint8_t fxmode) const {
  if (fxmode == 0)
    return nullptr;
  for (const auto &m : this->effects_) {
    for (uint8_t id : m.wled_ids) {
      if (id == fxmode)
        return m.effect;
    }
  }
  return nullptr;
}

// ── group masks ────────────────────────────────────────────────────────

void WledEmu::set_send_groups(uint8_t m) {
  if (m == this->send_groups_)
    return;
  this->send_groups_ = m;
  this->groups_cb_.call(this->send_groups_, this->recv_groups_);
}

void WledEmu::set_receive_groups(uint8_t m) {
  if (m == this->recv_groups_)
    return;
  this->recv_groups_ = m;
  this->groups_cb_.call(this->send_groups_, this->recv_groups_);
}

void WledEmu::set_group_bit(bool send, uint8_t group, bool on) {
  if (group < 1 || group > 8)
    return;
  uint8_t bit = 1u << (group - 1);
  uint8_t m = send ? this->send_groups_ : this->recv_groups_;
  m = on ? (m | bit) : (m & ~bit);
  if (send)
    this->set_send_groups(m);
  else
    this->set_receive_groups(m);
}

// ── sending ────────────────────────────────────────────────────────────

bool WledEmu::send_notifier(bool on, uint8_t bri, uint8_t r, uint8_t g, uint8_t b, uint8_t fxmode, uint8_t groups) {
  if (!this->sync_enable_) {
    ESP_LOGD(TAG, "send gated: sync disabled");
    return false;
  }
  uint8_t pkt[proto::NOTIFIER_LEN];
  proto::build_notifier(pkt, on, bri, r, g, b, fxmode, groups, false);
  bool ok = this->broadcast_(this->port_, pkt, sizeof(pkt));
  ESP_LOGD(TAG, "notifier on=%d bri=%u rgb=%u/%u/%u fx=%u grp=0x%02X -> %s", on, bri, r, g, b,
           fxmode, groups, ok ? "ok" : "FAIL");
  return ok;
}

void WledEmu::send_sysinfo() {
  uint8_t ip[4];
  if (!local_ip4(ip))
    return;
  bool on = false;
#ifdef USE_LIGHT
  if (this->light_ != nullptr)
    on = this->light_->remote_values.is_on();
#endif
  uint8_t pkt[proto::SYSINFO_LEN];
  proto::build_sysinfo(pkt, ip, App.get_name().c_str(), this->node_type_, on);
  this->broadcast_(this->info_port_, pkt, sizeof(pkt));
}

void WledEmu::send_state() {
#ifdef USE_LIGHT
  if (this->light_ == nullptr || !this->sync_enable_ || !this->sync_send_)
    return;
  bool on;
  uint8_t bri, r, g, b, fx;
  this->current_light_(on, bri, r, g, b, fx);
  uint8_t pkt[proto::NOTIFIER_LEN];
  proto::build_notifier(pkt, on, bri, r, g, b, fx, this->send_groups_, false);
  bool ok = this->broadcast_(this->port_, pkt, sizeof(pkt));
  ESP_LOGD(TAG, "state on=%d bri=%u rgb=%u/%u/%u fx=%u grp=0x%02X -> %s", on, bri, r, g, b, fx, this->send_groups_,
           ok ? "ok" : "FAIL");
  this->remember_state_();
  // Duplicate with the followUp byte set so peers can dedupe a lost packet.
  this->follow_up_at_ = this->retransmit_ ? (millis() + FOLLOW_UP_MS) | 1u : 0;
#endif
}

// ── discovery ──────────────────────────────────────────────────────────

void WledEmu::drain_info_port_() {
  uint8_t buf[80];
  uint8_t src[4];
  uint8_t self[4] = {0, 0, 0, 0};
  bool have_self = local_ip4(self);
  for (int guard = 0; guard < 16; guard++) {
    int n = this->info_.recv(buf, sizeof(buf), src);
    if (n <= 0)
      break;
    proto::SysInfo si;
    if (!proto::parse_sysinfo(buf, n, si))
      continue;
    if (have_self && memcmp(si.ip, self, 4) == 0)
      continue;
    char ip[16];
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", si.ip[0], si.ip[1], si.ip[2], si.ip[3]);
    WledNode *node = nullptr;
    for (auto &nd : this->nodes_) {
      if (nd.ip == ip) {
        node = &nd;
        break;
      }
    }
    bool fresh = false;
    if (node == nullptr) {
      if (this->nodes_.size() >= this->max_nodes_)
        continue;
      this->nodes_.emplace_back();
      node = &this->nodes_.back();
      node->ip = ip;
      fresh = true;
    }
    if (si.name[0] != 0)
      node->name = si.name;
    node->type = si.type;
    node->vid = si.vid;
    node->last_seen = millis();
    if (fresh) {
      ESP_LOGD(TAG, "node %s '%s' type=%u", node->ip.c_str(), node->name.c_str(), node->type);
      this->node_cb_.call(node->ip, node->name);
    }
  }
}

void WledEmu::age_nodes_() {
  uint32_t now = millis();
  for (int i = (int) this->nodes_.size() - 1; i >= 0; i--) {
    if (now - this->nodes_[i].last_seen > this->node_timeout_)
      this->nodes_.erase(this->nodes_.begin() + i);
  }
}

// ── light binding ──────────────────────────────────────────────────────
#ifdef USE_LIGHT

bool WledEmu::light_has_rgb_() {
  return this->light_->get_traits().supports_color_capability(light::ColorCapability::RGB);
}

void WledEmu::current_light_(bool &on, uint8_t &bri, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &fx) {
  auto &st = this->light_->remote_values;
  on = st.is_on();
  bri = on ? (uint8_t) (st.get_brightness() * 255.0f + 0.5f) : 0;
  if (this->light_has_rgb_()) {
    r = (uint8_t) (st.get_red() * 255.0f + 0.5f);
    g = (uint8_t) (st.get_green() * 255.0f + 0.5f);
    b = (uint8_t) (st.get_blue() * 255.0f + 0.5f);
  } else {
    // Monochrome: drive the peer's colour as white at our brightness.
    r = g = b = bri;
  }
  fx = on ? this->current_fxmode() : 0;
}

uint8_t WledEmu::current_fxmode() {
  StringRef name = this->light_->get_effect_name();
  return name.empty() ? 0 : this->fxmode_for_effect(name.str());
}

void WledEmu::apply_fxmode(light::LightCall &call, uint8_t fxmode) {
  const char *effect = this->effect_for_fxmode(fxmode);
  call.set_effect(effect != nullptr ? effect : "None");
}

void WledEmu::check_send_on_change_() {
  if (!this->have_last_sent_) {
    // First check after boot: take the light's state as the
    // baseline. Broadcasting it would make every peer in the send groups
    // copy whatever this node woke up with (usually "off").
    this->remember_state_();
    return;
  }
  if (!this->sync_enable_ || !this->sync_send_ || !this->send_on_change_)
    return;
  if (millis() - this->last_recv_ms_ < ECHO_COOLDOWN_MS)
    return;  // don't echo what we just applied
  // Brightness, colour and effect: any change is broadcast.
  if (this->light_snapshot_() == this->last_sent_)
    return;
  this->send_state();
}

uint64_t WledEmu::light_snapshot_() {
  bool on;
  uint8_t bri, r, g, b, fx;
  this->current_light_(on, bri, r, g, b, fx);
  if (!on)
    return 0;  // off: colour and effect do not matter
  return 1ull << 40 | (uint64_t) bri << 32 | (uint64_t) r << 24 | (uint64_t) g << 16 | (uint64_t) b << 8 | fx;
}

void WledEmu::remember_state_() {
  this->last_sent_ = this->light_snapshot_();
  this->have_last_sent_ = true;
}

void WledEmu::note_rx_(const uint8_t src[4], const char *kind, const char *result, const std::string &detail) {
  char ip[16];
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);
  this->rx_count_++;
  this->last_rx_at_ = millis() | 1u;
  this->last_rx_ip_ = ip;
  this->last_rx_kind_ = kind;
  this->last_rx_result_ = result;
  this->last_rx_detail_ = detail;
  ESP_LOGD(TAG, "rx %s from %s: %s %s", kind, ip, result, detail.c_str());
}

void WledEmu::drain_sync_port_() {
  static const char *const RT_NAMES[6] = {"", "WARLS", "DRGB", "DRGBW", "DNRGB", "DNRGBW"};
  uint8_t buf[256];
  uint8_t src[4];
  char d[48];
  for (int guard = 0; guard < 8; guard++) {
    int n = this->sync_.recv(buf, sizeof(buf), src);
    if (n <= 0)
      break;
    uint8_t p = buf[0];
    const char *gate = !this->sync_enable_ ? "sync off" : !this->sync_recv_ ? "receive off" : nullptr;
    if (p == 0) {
      proto::Notifier nt;
      if (!proto::parse_notifier(buf, n, nt)) {
        this->note_rx_(src, "notifier", "malformed");
        continue;
      }
      snprintf(d, sizeof(d), "bri %u rgb %u/%u/%u fx %u grp %02X", nt.bri, nt.r, nt.g, nt.b, nt.fxmode, nt.groups);
      if (gate != nullptr) {
        this->note_rx_(src, "notifier", gate, d);
      } else if ((this->recv_groups_ & nt.groups) == 0) {
        this->note_rx_(src, "notifier", "other group", d);
      } else {
        this->note_rx_(src, "notifier", "applied", d);
        this->on_notifier_(nt);
      }
    } else if (p >= 1 && p <= 5) {
      proto::Realtime rt;
      if (n < 2 || !proto::parse_realtime(buf, n, rt)) {
        this->note_rx_(src, RT_NAMES[p], "malformed");
        continue;
      }
      snprintf(d, sizeof(d), "rgb %u/%u/%u w %u, %us", rt.r, rt.g, rt.b, rt.w, rt.timeout_s);
      if (gate != nullptr) {
        this->note_rx_(src, RT_NAMES[p], gate, d);
      } else if (rt.timeout_s == 0) {
        this->note_rx_(src, RT_NAMES[p], "leave realtime", d);
        this->leave_realtime_();
      } else {
        this->note_rx_(src, RT_NAMES[p], "applied", d);
        this->on_realtime_(rt.r, rt.g, rt.b, rt.w, (uint32_t) rt.timeout_s * 1000u);
      }
    } else if (p == 0x9C && n >= 2) {
      // TPM2.NET: 0xAA = ping (answer 0xAC), 0xDA = data frame.
      uint8_t t = buf[1];
      if (t == 0xAA) {
        uint8_t ack = 0xAC;
        this->tx_.send_to(src, proto::TPM2_ACK_PORT, &ack, 1);
        this->note_rx_(src, "TPM2 ping", "answered");
      } else if (t == 0xDA && n >= 9 && buf[4] == 1) {
        snprintf(d, sizeof(d), "rgb %u/%u/%u", buf[6], buf[7], buf[8]);
        if (gate != nullptr) {
          this->note_rx_(src, "TPM2", gate, d);
        } else {
          this->note_rx_(src, "TPM2", "applied", d);
          this->on_realtime_(buf[6], buf[7], buf[8], 0, TPM2_HOLD_MS);
        }
      } else {
        this->note_rx_(src, "TPM2", "unsupported");
      }
    } else if (p == '{') {
      this->note_rx_(src, "JSON", "not supported");  // JSON API over UDP is not handled
    } else {
      snprintf(d, sizeof(d), "first byte 0x%02X, %d bytes", p, n);
      this->note_rx_(src, "unknown", "ignored", d);
    }
  }
}

void WledEmu::on_notifier_(const proto::Notifier &n) {
  this->last_recv_ms_ = millis();
  if (this->recv_bri_ && n.bri == 0) {
    this->light_->turn_off().perform();
    this->remember_state_();
    return;
  }
  auto call = this->light_->turn_on();
  bool any = false;
  if (this->recv_bri_) {
    call.set_brightness(n.bri / 255.0f);
    if (this->light_has_rgb_() && (n.r | n.g | n.b))
      call.set_rgb(n.r / 255.0f, n.g / 255.0f, n.b / 255.0f);
    any = true;
  }
  if (this->recv_effects_) {
    this->apply_fxmode(call, n.fxmode);
    any = true;
  }
  if (any)
    call.perform();
  this->remember_state_();  // what we just applied is not echoed back
}

void WledEmu::on_realtime_(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint32_t hold_ms) {
  if (this->rt_until_ms_ == 0) {
    // Entering realtime: remember what to restore when it times out.
    // Entering realtime: remember the whole state to restore on exit.
    auto &st = this->light_->remote_values;
    this->rt_prev_on_ = st.is_on();
    this->rt_prev_bri_ = st.get_brightness();
    this->rt_prev_rgb_[0] = st.get_red();
    this->rt_prev_rgb_[1] = st.get_green();
    this->rt_prev_rgb_[2] = st.get_blue();
    this->rt_prev_effect_ = this->light_->get_effect_name().str();
  }
  this->rt_until_ms_ = (millis() + hold_ms) | 1u;
  uint8_t v = r;
  if (g > v) v = g;
  if (b > v) v = b;
  if (w > v) v = w;
  if (v == 0) {
    this->light_->turn_off().set_transition_length(0).perform();
  } else {
    auto call = this->light_->turn_on();
    call.set_transition_length(0).set_effect("None");
    if (this->light_has_rgb_()) {
      call.set_rgb(r / (float) v, g / (float) v, b / (float) v);
    }
    call.set_brightness(v / 255.0f).perform();
  }
  this->remember_state_();
  this->last_recv_ms_ = millis();
}

void WledEmu::leave_realtime_() {
  if (this->rt_until_ms_ == 0)
    return;
  this->rt_until_ms_ = 0;
  if (this->rt_prev_on_ && this->rt_prev_bri_ > 0.0f) {
    auto call = this->light_->turn_on();
    call.set_brightness(this->rt_prev_bri_);
    if (this->light_has_rgb_())
      call.set_rgb(this->rt_prev_rgb_[0], this->rt_prev_rgb_[1], this->rt_prev_rgb_[2]);
    call.set_effect(this->rt_prev_effect_.empty() ? std::string("None") : this->rt_prev_effect_);
    call.perform();
  } else {
    this->light_->turn_off().perform();
  }
  this->last_recv_ms_ = millis();
}

#endif  // USE_LIGHT

}  // namespace wled_emu
}  // namespace esphome
