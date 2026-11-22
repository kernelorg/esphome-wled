"""wled_emu binary sensor: realtime (the light follows realtime data now)."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import WledEmu, wled_emu_ns

DEPENDENCIES = ["wled_emu"]
CONF_WLED_EMU_ID = "wled_emu_id"

WledBinarySensor = wled_emu_ns.class_("WledBinarySensor", binary_sensor.BinarySensor, cg.Component)
WledBinarySensorType = wled_emu_ns.enum("WledBinarySensorType", is_class=True)
TYPES = {"realtime": WledBinarySensorType.REALTIME}

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(WledBinarySensor, icon="mdi:television-play")
    .extend(
        {
            cv.GenerateID(CONF_WLED_EMU_ID): cv.use_id(WledEmu),
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_WLED_EMU_ID])
    cg.add(var.set_type(config[CONF_TYPE]))
