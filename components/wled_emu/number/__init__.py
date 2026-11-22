"""wled_emu numbers: port (sync / realtime UDP) and info_port (discovery).

The value is saved (restore_value, default true); a change reopens the
socket on the next loop.
"""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_RESTORE_VALUE, CONF_TYPE

from .. import WledEmu, wled_emu_ns

DEPENDENCIES = ["wled_emu"]
CONF_WLED_EMU_ID = "wled_emu_id"

WledNumber = wled_emu_ns.class_("WledNumber", number.Number, cg.Component)
WledNumberType = wled_emu_ns.enum("WledNumberType", is_class=True)
TYPES = {"port": WledNumberType.PORT, "info_port": WledNumberType.INFO_PORT}

CONFIG_SCHEMA = (
    number.number_schema(WledNumber, icon="mdi:numeric")
    .extend(
        {
            cv.GenerateID(CONF_WLED_EMU_ID): cv.use_id(WledEmu),
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
            cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await number.new_number(config, min_value=1, max_value=65535, step=1)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_WLED_EMU_ID])
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))
