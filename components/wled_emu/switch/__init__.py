"""Switches for the wled_emu runtime settings and sync groups.

    switch:
      - platform: wled_emu
        type: sync_send          # sync | sync_send | sync_receive |
        name: "Sync send"        # receive_brightness | receive_effects |
                                 # send_on_change | retransmit |
      - platform: wled_emu       # send_group | receive_group (+ group: 1..8)
        type: receive_group
        group: 2
        name: "Receive group 2"

The state is saved (restore_mode, default RESTORE_DEFAULT_OFF): after a
reboot the saved value is applied; with nothing saved yet the value from
the wled_emu config stays. ALWAYS_ON / ALWAYS_OFF force a value at boot.
Changes made elsewhere (the /sync page, the debug UI, lambdas) are shown.
"""

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import WledEmu, wled_emu_ns

DEPENDENCIES = ["wled_emu"]

CONF_WLED_EMU_ID = "wled_emu_id"
CONF_GROUP = "group"

WledSwitch = wled_emu_ns.class_("WledSwitch", switch.Switch, cg.Component)
WledSwitchType = wled_emu_ns.enum("WledSwitchType", is_class=True)
TYPES = {
    "sync": WledSwitchType.SYNC,
    "sync_send": WledSwitchType.SYNC_SEND,
    "sync_receive": WledSwitchType.SYNC_RECEIVE,
    "receive_brightness": WledSwitchType.RECEIVE_BRIGHTNESS,
    "receive_effects": WledSwitchType.RECEIVE_EFFECTS,
    "send_on_change": WledSwitchType.SEND_ON_CHANGE,
    "retransmit": WledSwitchType.RETRANSMIT,
    "send_group": WledSwitchType.SEND_GROUP,
    "receive_group": WledSwitchType.RECEIVE_GROUP,
}


def _validate_group(config):
    is_group = config[CONF_TYPE].endswith("_group")
    if is_group and CONF_GROUP not in config:
        raise cv.Invalid(f"'{CONF_GROUP}: 1..8' is required for type {config[CONF_TYPE]}")
    if not is_group and CONF_GROUP in config:
        raise cv.Invalid(f"'{CONF_GROUP}' only applies to send_group / receive_group")
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(WledSwitch, default_restore_mode="RESTORE_DEFAULT_OFF")
    .extend(
        {
            cv.GenerateID(CONF_WLED_EMU_ID): cv.use_id(WledEmu),
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
            cv.Optional(CONF_GROUP): cv.int_range(min=1, max=8),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_group,
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_WLED_EMU_ID])
    cg.add(var.set_type(config[CONF_TYPE]))
    if CONF_GROUP in config:
        cg.add(var.set_group(config[CONF_GROUP]))
