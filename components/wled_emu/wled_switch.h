#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "wled_emu.h"

namespace esphome {
namespace wled_emu {

enum class WledSwitchType : uint8_t {
  SYNC,
  SYNC_SEND,
  SYNC_RECEIVE,
  RECEIVE_BRIGHTNESS,
  RECEIVE_EFFECTS,
  SEND_ON_CHANGE,
  RETRANSMIT,
  SEND_GROUP,
  RECEIVE_GROUP,
};

/// One wled_emu runtime setting (or one sync-group bit) as a switch.
class WledSwitch : public switch_::Switch, public Component, public Parented<WledEmu> {
 public:
  void set_type(WledSwitchType t) { this->type_ = t; }
  void set_group(uint8_t g) { this->group_ = g; }

  void setup() override;
  void dump_config() override;
  // Before wled_emu starts sending, after its configuration is set.
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void write_state(bool state) override;
  bool current_() const;

  WledSwitchType type_{WledSwitchType::SYNC};
  uint8_t group_{1};
};

}  // namespace wled_emu
}  // namespace esphome

#endif  // USE_SWITCH
