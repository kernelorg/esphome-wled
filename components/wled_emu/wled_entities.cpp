#include "wled_entities.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wled_emu {

static const char *const TAG = "wled_emu.entity";

// ── sensor ─────────────────────────────────────────────────────────────
#ifdef USE_SENSOR
void WledSensor::loop() {
  uint32_t now = millis();
  if (this->published_ && now - this->last_check_ < ENTITY_CHECK_MS)
    return;
  this->last_check_ = now;
  float v = 0;
  switch (this->type_) {
    case WledSensorType::PEERS: v = (float) this->parent_->node_count(); break;
    case WledSensorType::RX_PACKETS: v = (float) this->parent_->rx_count(); break;
    case WledSensorType::TX_PACKETS: v = (float) this->parent_->tx_count(); break;
  }
  if (!this->published_ || v != this->state) {
    this->published_ = true;
    this->publish_state(v);
  }
}
void WledSensor::dump_config() { LOG_SENSOR("", "WLED sensor", this); }
#endif

// ── binary sensor ──────────────────────────────────────────────────────
#ifdef USE_BINARY_SENSOR
void WledBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "WLED binary sensor", this); }
#endif

// ── text sensor ────────────────────────────────────────────────────────
#ifdef USE_TEXT_SENSOR
std::string WledTextSensor::value_() {
  auto *w = this->parent_;
  switch (this->type_) {
    case WledTextSensorType::LAST_PACKET: {
      if (w->last_rx_kind().empty())
        return "";
      std::string s = w->last_rx_kind() + " " + w->last_rx_result();
      if (!w->last_rx_detail().empty())
        s += ": " + w->last_rx_detail();
      return s;
    }
    case WledTextSensorType::LAST_SENDER:
      return w->last_rx_ip();
    case WledTextSensorType::PEERS: {
      std::string s;
      for (const auto &n : w->nodes()) {
        std::string item = (n.name.empty() ? std::string("?") : n.name) + " (" + n.ip + ")";
        if (s.size() + item.size() + 2 > 255)
          break;  // Home Assistant state limit
        if (!s.empty())
          s += ", ";
        s += item;
      }
      return s;
    }
  }
  return "";
}

void WledTextSensor::loop() {
  uint32_t now = millis();
  if (this->published_ && now - this->last_check_ < ENTITY_CHECK_MS)
    return;
  this->last_check_ = now;
  std::string v = this->value_();
  if (!this->published_ || v != this->state) {
    this->published_ = true;
    this->publish_state(v);
  }
}
void WledTextSensor::dump_config() { LOG_TEXT_SENSOR("", "WLED text sensor", this); }
#endif

// ── number ─────────────────────────────────────────────────────────────
#ifdef USE_NUMBER
uint16_t WledNumber::current_() const {
  return this->type_ == WledNumberType::PORT ? this->parent_->get_port() : this->parent_->get_info_port();
}

void WledNumber::setup() {
  float value = this->current_();
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<float>();
    float saved;
    if (this->pref_.load(&saved) && saved >= 1 && saved <= 65535)
      value = saved;
  }
  this->control(value);
}

void WledNumber::control(float value) {
  auto port = (uint16_t) value;
  if (this->type_ == WledNumberType::PORT)
    this->parent_->set_port(port);
  else
    this->parent_->set_info_port(port);
  this->publish_state(port);
  if (this->restore_value_)
    this->pref_.save(&value);
}

void WledNumber::dump_config() {
  LOG_NUMBER("", "WLED number", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}
#endif

}  // namespace wled_emu
}  // namespace esphome
