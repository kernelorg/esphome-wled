"""wled_emu sensors: peers (WLED nodes heard), rx_packets, tx_packets."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, STATE_CLASS_MEASUREMENT, STATE_CLASS_TOTAL_INCREASING

from .. import WledEmu, wled_emu_ns

DEPENDENCIES = ["wled_emu"]
CONF_WLED_EMU_ID = "wled_emu_id"

WledSensor = wled_emu_ns.class_("WledSensor", sensor.Sensor, cg.Component)
WledSensorType = wled_emu_ns.enum("WledSensorType", is_class=True)


def _schema(type_value, icon, state_class):
    return (
        sensor.sensor_schema(WledSensor, icon=icon, accuracy_decimals=0, state_class=state_class)
        .extend({cv.GenerateID(CONF_WLED_EMU_ID): cv.use_id(WledEmu)})
        .extend(cv.COMPONENT_SCHEMA)
    )


TYPES = {
    "peers": (WledSensorType.PEERS, "mdi:lan", STATE_CLASS_MEASUREMENT),
    "rx_packets": (WledSensorType.RX_PACKETS, "mdi:download-network", STATE_CLASS_TOTAL_INCREASING),
    "tx_packets": (WledSensorType.TX_PACKETS, "mdi:upload-network", STATE_CLASS_TOTAL_INCREASING),
}

CONFIG_SCHEMA = cv.typed_schema(
    {k: _schema(*v) for k, v in TYPES.items()}, key=CONF_TYPE, lower=True
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_WLED_EMU_ID])
    cg.add(var.set_type(TYPES[config[CONF_TYPE]][0]))
