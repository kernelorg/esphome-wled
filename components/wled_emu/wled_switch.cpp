#include "wled_switch.h"
#ifdef USE_SWITCH

#include "esphome/core/log.h"

namespace esphome {
namespace wled_emu {

static const char *const TAG = "wled_emu.switch";

bool WledSwitch::current_() const {
  auto *w = this->parent_;
  uint8_t bit = 1u << (this->group_ - 1);
  switch (this->type_) {
    case WledSwitchType::SYNC: return w->get_sync_enable();
    case WledSwitchType::SYNC_SEND: return w->get_sync_send();
    case WledSwitchType::SYNC_RECEIVE: return w->get_sync_receive();
    case WledSwitchType::RECEIVE_BRIGHTNESS: return w->get_receive_brightness();
    case WledSwitchType::RECEIVE_EFFECTS: return w->get_receive_effects();
    case WledSwitchType::SEND_ON_CHANGE: return w->get_send_on_change();
    case WledSwitchType::RETRANSMIT: return w->get_retransmit();
    case WledSwitchType::SEND_GROUP: return (w->get_send_groups() & bit) != 0;
    case WledSwitchType::RECEIVE_GROUP: return (w->get_receive_groups() & bit) != 0;
  }
  return false;
}

void WledSwitch::write_state(bool state) {
  auto *w = this->parent_;
  switch (this->type_) {
    case WledSwitchType::SYNC: w->set_sync_enable(state); break;
    case WledSwitchType::SYNC_SEND: w->set_sync_send(state); break;
    case WledSwitchType::SYNC_RECEIVE: w->set_sync_receive(state); break;
    case WledSwitchType::RECEIVE_BRIGHTNESS: w->set_receive_brightness(state); break;
    case WledSwitchType::RECEIVE_EFFECTS: w->set_receive_effects(state); break;
    case WledSwitchType::SEND_ON_CHANGE: w->set_send_on_change(state); break;
    case WledSwitchType::RETRANSMIT: w->set_retransmit(state); break;
    case WledSwitchType::SEND_GROUP: w->set_group_bit(true, this->group_, state); break;
    case WledSwitchType::RECEIVE_GROUP: w->set_group_bit(false, this->group_, state); break;
  }
  this->publish_state(this->current_());
}

void WledSwitch::setup() {
  // Saved value → apply; ALWAYS_ON/OFF → apply; nothing saved → keep the
  // value from the wled_emu config.
  optional<bool> initial;
  const char *source = "config";
  if (this->restore_mode & switch_::RESTORE_MODE_PERSISTENT_MASK) {
    auto saved = this->get_initial_state();
    if (saved.has_value()) {
      initial = (this->restore_mode & switch_::RESTORE_MODE_INVERTED_MASK) ? !*saved : *saved;
      source = "restored";
    }
  } else if (!(this->restore_mode & switch_::RESTORE_MODE_DISABLED_MASK)) {
    initial = (this->restore_mode & switch_::RESTORE_MODE_ON_MASK) != 0;
    source = "restore_mode";
  }
  ESP_LOGD(TAG, "'%s': %s (%s)", this->get_name().c_str(),
           (initial.has_value() ? *initial : this->current_()) ? "ON" : "OFF", source);
  if (initial.has_value())
    this->write_state(*initial);
  else
    this->publish_state(this->current_());

  // Follow changes made elsewhere (/sync page, debug UI, lambdas).
  this->parent_->add_on_settings_change_callback([this]() {
    if (this->current_() != this->state)
      this->publish_state(this->current_());
  });
  this->parent_->add_on_groups_change_callback([this](uint8_t, uint8_t) {
    if (this->current_() != this->state)
      this->publish_state(this->current_());
  });
}

void WledSwitch::dump_config() { LOG_SWITCH("", "WLED switch", this); }

}  // namespace wled_emu
}  // namespace esphome

#endif  // USE_SWITCH
