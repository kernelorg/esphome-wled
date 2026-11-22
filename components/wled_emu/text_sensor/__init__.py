"""wled_emu text sensors: last_packet, last_sender, peers."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import WledEmu, wled_emu_ns

DEPENDENCIES = ["wled_emu"]
CONF_WLED_EMU_ID = "wled_emu_id"

WledTextSensor = wled_emu_ns.class_("WledTextSensor", text_sensor.TextSensor, cg.Component)
WledTextSensorType = wled_emu_ns.enum("WledTextSensorType", is_class=True)


def _schema(icon):
    return (
        text_sensor.text_sensor_schema(WledTextSensor, icon=icon)
        .extend({cv.GenerateID(CONF_WLED_EMU_ID): cv.use_id(WledEmu)})
        .extend(cv.COMPONENT_SCHEMA)
    )


TYPES = {
    "last_packet": (WledTextSensorType.LAST_PACKET, "mdi:message-text"),
    "last_sender": (WledTextSensorType.LAST_SENDER, "mdi:ip-network"),
    "peers": (WledTextSensorType.PEERS, "mdi:lan"),
}

CONFIG_SCHEMA = cv.typed_schema({k: _schema(v[1]) for k, v in TYPES.items()}, key=CONF_TYPE, lower=True)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_WLED_EMU_ID])
    cg.add(var.set_type(TYPES[config[CONF_TYPE]][0]))
