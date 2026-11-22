#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "udp_transport.h"
#include "wled_protocol.h"

#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif

#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace wled_emu {

/// One WLED peer heard via node-info (sysInfo) broadcasts.
struct WledNode {
  std::string ip;
  std::string name;
  uint8_t type{0};  // bit 7 = peer reports its light ON
  uint32_t vid{0};
  uint32_t last_seen{0};
};

class WledEmu : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // ── configuration (codegen + runtime) ──────────────────────────────
  void set_port(uint16_t p) { this->port_ = p; }
  void set_info_port(uint16_t p) { this->info_port_ = p; }
  uint16_t get_port() const { return this->port_; }
  uint16_t get_info_port() const { return this->info_port_; }
  void set_node_type(uint8_t t) { this->node_type_ = t; }
  void set_discovery(bool d) { this->discovery_ = d; }
  void set_sysinfo_interval(uint32_t ms) { this->sysinfo_interval_ = ms; }
  void set_node_timeout(uint32_t ms) { this->node_timeout_ = ms; }
  void set_max_nodes(uint8_t n) { this->max_nodes_ = n; }

  // Runtime switches; a change notifies add_on_settings_change_callback().
  void set_sync_enable(bool v) { this->set_flag_(this->sync_enable_, v); }
  void set_sync_send(bool v) { this->set_flag_(this->sync_send_, v); }
  void set_sync_receive(bool v) { this->set_flag_(this->sync_recv_, v); }
  void set_receive_brightness(bool v) { this->set_flag_(this->recv_bri_, v); }
  void set_receive_effects(bool v) { this->set_flag_(this->recv_effects_, v); }
  void set_send_on_change(bool v) { this->set_flag_(this->send_on_change_, v); }
  void set_retransmit(bool v) { this->set_flag_(this->retransmit_, v); }
  void set_send_groups(uint8_t m);
  void set_receive_groups(uint8_t m);
  /// Set/clear one group bit (group = 1..8) of the send or receive mask.
  void set_group_bit(bool send, uint8_t group, bool on);

  bool get_sync_enable() const { return this->sync_enable_; }
  bool get_sync_send() const { return this->sync_send_; }
  bool get_sync_receive() const { return this->sync_recv_; }
  bool get_receive_brightness() const { return this->recv_bri_; }
  bool get_receive_effects() const { return this->recv_effects_; }
  bool get_send_on_change() const { return this->send_on_change_; }
  bool get_retransmit() const { return this->retransmit_; }
  uint8_t get_send_groups() const { return this->send_groups_; }
  uint8_t get_receive_groups() const { return this->recv_groups_; }

  /// Effect table (config `effects:`): this light effect ↔ these WLED
  /// effect IDs. Receive: any of the IDs plays the effect; send: the effect
  /// goes out as the first ID.
  void add_effect(const char *effect, std::vector<uint8_t> wled_ids) {
    this->effects_.push_back({effect, std::move(wled_ids)});
  }
  /// WLED effect ID for an ESPHome effect name (0 if not in the table).
  uint8_t fxmode_for_effect(const std::string &effect) const;
  /// ESPHome effect for a WLED effect ID (nullptr = none / not in the table).
  const char *effect_for_fxmode(uint8_t fxmode) const;

#ifdef USE_LIGHT
  void set_light(light::LightState *l) { this->light_ = l; }
  light::LightState *get_light() const { return this->light_; }
  /// WLED effect ID to send for the light's current effect (0 = Solid / no mapping).
  uint8_t current_fxmode();
  /// Set the light effect for a received WLED effect ID ("None" if unmapped).
  void apply_fxmode(light::LightCall &call, uint8_t fxmode);
#endif

  // ── sending ────────────────────────────────────────────────────────
  /// Broadcast a notifier built from explicit values (does not touch any
  /// bound light). `fxmode` is the WLED effect ID (0 = Solid).
  bool send_notifier(bool on, uint8_t bri, uint8_t r, uint8_t g, uint8_t b, uint8_t fxmode, uint8_t groups);
  bool send_notifier(bool on, uint8_t bri, uint8_t r, uint8_t g, uint8_t b, uint8_t fxmode) {
    return this->send_notifier(on, bri, r, g, b, fxmode, this->send_groups_);
  }
  /// Broadcast the bound light's current state (+ optional follow-up).
  void send_state();
  void send_sysinfo();

  // ── discovery ──────────────────────────────────────────────────────
  const std::vector<WledNode> &nodes() const { return this->nodes_; }
  size_t node_count() const { return this->nodes_.size(); }

  // ── statistics (debug UI / lambdas) ────────────────────────────────
  /// Packets received on the sync port (all kinds, also ignored ones).
  uint32_t rx_count() const { return this->rx_count_; }
  /// Packets broadcast (notifiers, follow-ups, node-info).
  uint32_t tx_count() const { return this->tx_count_; }
  /// Last packet on the sync port: "notifier", "WARLS", "DRGB", "DRGBW",
  /// "DNRGB", "DNRGBW", "TPM2", "TPM2 ping", "JSON", "unknown"; "" = none yet.
  const std::string &last_rx_kind() const { return this->last_rx_kind_; }
  /// What happened to it: "applied", "other group", "sync off", "receive off", ...
  const std::string &last_rx_result() const { return this->last_rx_result_; }
  /// Decoded values, e.g. "bri 200 rgb 255/0/0 fx 2 grp 01".
  const std::string &last_rx_detail() const { return this->last_rx_detail_; }
  const std::string &last_rx_ip() const { return this->last_rx_ip_; }
  /// ms since the last packet on the sync port (UINT32_MAX = none yet).
  uint32_t last_rx_age_ms() const {
    return this->last_rx_at_ == 0 ? UINT32_MAX : millis() - this->last_rx_at_;
  }
  /// The bound light is driven by realtime data right now.
  bool in_realtime() const {
#ifdef USE_LIGHT
    return this->rt_until_ms_ != 0;
#else
    return false;
#endif
  }

  /// Extra work for the HTTP API (WebSocket), run from loop().
  void set_http_loop(std::function<void()> &&f) { this->http_loop_ = std::move(f); }

  /// Local IPv4 (false if not connected).
  static bool local_ip4(uint8_t out[4]);

  /// Any of the runtime switches (sync, send, receive, …) changed.
  void add_on_settings_change_callback(std::function<void()> &&cb) { this->settings_cb_.add(std::move(cb)); }
  void add_on_groups_change_callback(std::function<void(uint8_t, uint8_t)> &&cb) {
    this->groups_cb_.add(std::move(cb));
  }
  void add_on_node_seen_callback(std::function<void(std::string, std::string)> &&cb) {
    this->node_cb_.add(std::move(cb));
  }

 protected:
  void ensure_sockets_();
  void drain_info_port_();
  void age_nodes_();
#ifdef USE_LIGHT
  void drain_sync_port_();
  void on_notifier_(const proto::Notifier &n);
  void on_realtime_(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint32_t hold_ms);
  void leave_realtime_();
  void check_send_on_change_();
  void current_light_(bool &on, uint8_t &bri, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &fx);
  bool light_has_rgb_();
  uint64_t light_snapshot_();
  void remember_state_();
#endif

  uint16_t port_{21324};
  uint16_t info_port_{65506};
  uint8_t node_type_{0};
  bool discovery_{true};
  uint32_t sysinfo_interval_{10000};
  uint32_t node_timeout_{60000};
  uint8_t max_nodes_{16};

  bool sync_enable_{true};
  bool sync_send_{false};
  bool sync_recv_{true};
  bool recv_bri_{true};
  bool recv_effects_{true};
  bool send_on_change_{true};
  bool retransmit_{true};
  uint8_t send_groups_{0x01};
  uint8_t recv_groups_{0x01};

  struct EffectMap {
    const char *effect;
    std::vector<uint8_t> wled_ids;
  };
  std::vector<EffectMap> effects_;

  UdpEndpoint tx_;    // ephemeral port, broadcast sender
  UdpEndpoint sync_;  // `port_`, only when a light is bound
  UdpEndpoint info_;  // `info_port_`, discovery

  std::vector<WledNode> nodes_;
  uint32_t last_sysinfo_ms_{0};
  uint32_t last_age_ms_{0};

#ifdef USE_LIGHT
  light::LightState *light_{nullptr};
  uint32_t rt_until_ms_{0};
  bool rt_prev_on_{false};
  float rt_prev_bri_{0.0f};
  float rt_prev_rgb_[3]{1.0f, 1.0f, 1.0f};
  std::string rt_prev_effect_;
  uint32_t last_recv_ms_{0};
  uint32_t last_change_check_ms_{0};
  uint64_t last_sent_{0};  // light_snapshot_() of the last state sent / applied
  bool have_last_sent_{false};
  uint32_t follow_up_at_{0};
#endif

  bool broadcast_(uint16_t port, const uint8_t *data, size_t len) {
    bool ok = this->tx_.broadcast(port, data, len);
    if (ok)
      this->tx_count_++;
    return ok;
  }
  void note_rx_(const uint8_t src[4], const char *kind, const char *result, const std::string &detail = "");

  std::function<void()> http_loop_;
  uint32_t rx_count_{0};
  uint32_t tx_count_{0};
  uint32_t last_rx_at_{0};
  std::string last_rx_kind_;
  std::string last_rx_result_;
  std::string last_rx_detail_;
  std::string last_rx_ip_;

  void set_flag_(bool &flag, bool v) {
    if (flag == v)
      return;
    flag = v;
    this->settings_cb_.call();
  }
  CallbackManager<void()> settings_cb_;
  CallbackManager<void(uint8_t, uint8_t)> groups_cb_;
  CallbackManager<void(std::string, std::string)> node_cb_;
};

// ── automation ─────────────────────────────────────────────────────────

class GroupsChangeTrigger : public Trigger<uint8_t, uint8_t> {
 public:
  explicit GroupsChangeTrigger(WledEmu *parent) {
    parent->add_on_groups_change_callback([this](uint8_t s, uint8_t r) { this->trigger(s, r); });
  }
};

class NodeSeenTrigger : public Trigger<std::string, std::string> {
 public:
  explicit NodeSeenTrigger(WledEmu *parent) {
    parent->add_on_node_seen_callback([this](std::string ip, std::string name) { this->trigger(ip, name); });
  }
};

template<typename... Ts> class SendNotifierAction : public Action<Ts...>, public Parented<WledEmu> {
 public:
  TEMPLATABLE_VALUE(bool, on)
  TEMPLATABLE_VALUE(uint8_t, brightness)
  TEMPLATABLE_VALUE(uint8_t, red)
  TEMPLATABLE_VALUE(uint8_t, green)
  TEMPLATABLE_VALUE(uint8_t, blue)
  TEMPLATABLE_VALUE(std::string, effect)    // ESPHome effect name → its WLED ID (effects table)
  TEMPLATABLE_VALUE(uint8_t, wled_effect)    // or a WLED effect ID directly
  TEMPLATABLE_VALUE(uint8_t, groups)

  void play(const Ts &...x) override {
    uint8_t groups = this->groups_.has_value() ? this->groups_.value(x...) : this->parent_->get_send_groups();
    uint8_t fx = 0;
    if (this->wled_effect_.has_value())
      fx = this->wled_effect_.value(x...);
    else if (this->effect_.has_value())
      fx = this->parent_->fxmode_for_effect(this->effect_.value(x...));
    this->parent_->send_notifier(this->on_.value_or(x..., true), this->brightness_.value_or(x..., 255),
                                 this->red_.value_or(x..., 255), this->green_.value_or(x..., 255),
                                 this->blue_.value_or(x..., 255), fx, groups);
  }
};

template<typename... Ts> class SendStateAction : public Action<Ts...>, public Parented<WledEmu> {
 public:
  void play(const Ts &...x) override { this->parent_->send_state(); }
};

}  // namespace wled_emu
}  // namespace esphome
