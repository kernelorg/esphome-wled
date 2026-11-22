#pragma once

// Sensor / binary sensor / text sensor / number platforms of wled_emu.
// Each entity checks its value in loop() and publishes only on change.

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "wled_emu.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#include "esphome/core/preferences.h"
#endif

namespace esphome {
namespace wled_emu {

static constexpr uint32_t ENTITY_CHECK_MS = 1000;

#ifdef USE_SENSOR
enum class WledSensorType : uint8_t { PEERS, RX_PACKETS, TX_PACKETS };

class WledSensor : public sensor::Sensor, public Component, public Parented<WledEmu> {
 public:
  void set_type(WledSensorType t) { this->type_ = t; }
  void loop() override;
  void dump_config() override;

 protected:
  WledSensorType type_{WledSensorType::PEERS};
  uint32_t last_check_{0};
  bool published_{false};
};
#endif

#ifdef USE_BINARY_SENSOR
enum class WledBinarySensorType : uint8_t { REALTIME };

class WledBinarySensor : public binary_sensor::BinarySensor, public Component, public Parented<WledEmu> {
 public:
  void set_type(WledBinarySensorType t) { this->type_ = t; }
  void setup() override { this->publish_initial_state(false); }
  void loop() override { this->publish_state(this->parent_->in_realtime()); }
  void dump_config() override;

 protected:
  WledBinarySensorType type_{WledBinarySensorType::REALTIME};
};
#endif

#ifdef USE_TEXT_SENSOR
enum class WledTextSensorType : uint8_t { LAST_PACKET, LAST_SENDER, PEERS };

class WledTextSensor : public text_sensor::TextSensor, public Component, public Parented<WledEmu> {
 public:
  void set_type(WledTextSensorType t) { this->type_ = t; }
  void loop() override;
  void dump_config() override;

 protected:
  std::string value_();

  WledTextSensorType type_{WledTextSensorType::LAST_PACKET};
  uint32_t last_check_{0};
  bool published_{false};
};
#endif

#ifdef USE_NUMBER
enum class WledNumberType : uint8_t { PORT, INFO_PORT };

class WledNumber : public number::Number, public Component, public Parented<WledEmu> {
 public:
  void set_type(WledNumberType t) { this->type_ = t; }
  void set_restore_value(bool r) { this->restore_value_ = r; }
  void setup() override;
  void dump_config() override;
  // After wled_emu's configuration is set, before its sockets open.
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
  uint16_t current_() const;

  WledNumberType type_{WledNumberType::PORT};
  bool restore_value_{true};
  ESPPreferenceObject pref_;
};
#endif

}  // namespace wled_emu
}  // namespace esphome
