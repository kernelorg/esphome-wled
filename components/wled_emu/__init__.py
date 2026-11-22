"""WLED-compatible node emulation for ESPHome.

UDP sync notifier (send/receive, sync groups, follow-up retransmit),
realtime (WARLS/DRGB/DRGBW/DNRGB/DNRGBW, TPM2.NET), node-info discovery
and an optional WLED HTTP/JSON API on web_server_base.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import socket
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT, CONF_TRIGGER_ID
from esphome.core import CORE

CODEOWNERS = ["@kernelorg"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket", "json"]

CONF_LIGHT_ID = "light_id"
CONF_INFO_PORT = "info_port"
CONF_HTTP_API = "http_api"
CONF_NODE_TYPE = "node_type"
CONF_DISCOVERY = "discovery"
CONF_SYSINFO_INTERVAL = "sysinfo_interval"
CONF_NODE_TIMEOUT = "node_timeout"
CONF_MAX_NODES = "max_nodes"
CONF_SYNC_ENABLE = "sync_enable"
CONF_SYNC_SEND = "sync_send"
CONF_SYNC_RECEIVE = "sync_receive"
CONF_RECEIVE_BRIGHTNESS = "receive_brightness"
CONF_RECEIVE_EFFECTS = "receive_effects"
CONF_SEND_ON_CHANGE = "send_on_change"
CONF_RETRANSMIT = "retransmit"
CONF_SEND_GROUPS = "send_groups"
CONF_RECEIVE_GROUPS = "receive_groups"
CONF_PULSE_EFFECT = "pulse_effect"
CONF_STROBE_EFFECT = "strobe_effect"
CONF_EFFECTS = "effects"
CONF_WLED = "wled"
CONF_WLED_EFFECT = "wled_effect"
CONF_ON_GROUPS_CHANGE = "on_groups_change"
CONF_ON_NODE_SEEN = "on_node_seen"

CONF_ON = "on"
CONF_BRIGHTNESS = "brightness"
CONF_RED = "red"
CONF_GREEN = "green"
CONF_BLUE = "blue"
CONF_EFFECT = "effect"
CONF_GROUPS = "groups"

wled_emu_ns = cg.esphome_ns.namespace("wled_emu")
WledEmu = wled_emu_ns.class_("WledEmu", cg.Component)
GroupsChangeTrigger = wled_emu_ns.class_(
    "GroupsChangeTrigger", automation.Trigger.template(cg.uint8, cg.uint8)
)
NodeSeenTrigger = wled_emu_ns.class_(
    "NodeSeenTrigger", automation.Trigger.template(cg.std_string, cg.std_string)
)
SendNotifierAction = wled_emu_ns.class_("SendNotifierAction", automation.Action)
SendStateAction = wled_emu_ns.class_("SendStateAction", automation.Action)

light_ns = cg.esphome_ns.namespace("light")
LightState = light_ns.class_("LightState", cg.Component)


def _validate(config):
    if config[CONF_HTTP_API]:
        if CONF_LIGHT_ID not in config:
            raise cv.Invalid(f"'{CONF_HTTP_API}' requires '{CONF_LIGHT_ID}'")
        if "web_server_base" not in CORE.loaded_integrations:
            raise cv.Invalid(
                f"'{CONF_HTTP_API}' requires the 'web_server' (or web_server_base) component"
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WledEmu),
            cv.Optional(CONF_LIGHT_ID): cv.use_id(LightState),
            cv.Optional(CONF_PORT, default=21324): cv.port,
            # WLED v16 binds node-info to 65506 (older builds: udpPort+1).
            cv.Optional(CONF_INFO_PORT, default=65506): cv.port,
            cv.Optional(CONF_HTTP_API, default=False): cv.boolean,
            cv.Optional(CONF_NODE_TYPE): cv.uint8_t,
            cv.Optional(CONF_DISCOVERY, default=True): cv.boolean,
            cv.Optional(
                CONF_SYSINFO_INTERVAL, default="10s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_NODE_TIMEOUT, default="60s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_NODES, default=16): cv.int_range(min=1, max=64),
            cv.Optional(CONF_SYNC_ENABLE, default=True): cv.boolean,
            cv.Optional(CONF_SYNC_SEND, default=False): cv.boolean,
            cv.Optional(CONF_SYNC_RECEIVE, default=True): cv.boolean,
            cv.Optional(CONF_RECEIVE_BRIGHTNESS, default=True): cv.boolean,
            cv.Optional(CONF_RECEIVE_EFFECTS, default=True): cv.boolean,
            cv.Optional(CONF_SEND_ON_CHANGE, default=True): cv.boolean,
            cv.Optional(CONF_RETRANSMIT, default=True): cv.boolean,
            cv.Optional(CONF_SEND_GROUPS, default=0x01): cv.uint8_t,
            cv.Optional(CONF_RECEIVE_GROUPS, default=0x01): cv.uint8_t,
            # Light effect <-> WLED effect IDs (see WLED_EFFECTS.md).
            cv.Optional(CONF_EFFECTS, default=[]): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_EFFECT): cv.string_strict,
                        cv.Required(CONF_WLED): cv.All(
                            cv.ensure_list(cv.int_range(min=1, max=255)), cv.Length(min=1)
                        ),
                    }
                )
            ),
            cv.Optional(CONF_PULSE_EFFECT): cv.invalid(
                "pulse_effect was replaced by the 'effects' list: an entry with "
                "effect: Pulse and wled: 2 (WLED Breathe); IDs are listed in WLED_EFFECTS.md"
            ),
            cv.Optional(CONF_STROBE_EFFECT): cv.invalid(
                "strobe_effect was replaced by the 'effects' list: an entry with "
                "effect: Strobe and wled: 23 (WLED Strobe); IDs are listed in WLED_EFFECTS.md"
            ),
            cv.Optional(CONF_ON_GROUPS_CHANGE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(GroupsChangeTrigger)}
            ),
            cv.Optional(CONF_ON_NODE_SEEN): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NodeSeenTrigger)}
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(["esp32", "esp8266", "rp2040", "bk72xx", "rtl87xx", "ln882x", "host"]),
    # 1 broadcast sender + sync listener + info listener (worst case).
    socket.consume_sockets(3, "wled_emu", socket.SocketType.UDP),
    _validate,
)


def _final_validate(config):
    """Every `effects:` entry must name an effect of the bound light."""
    import esphome.final_validate as fv

    if not config[CONF_EFFECTS]:
        return config
    if CONF_LIGHT_ID not in config:
        return config  # send-only: the names are used by send_notifier only
    full = fv.full_config.get()
    light_conf = None
    for lc in full.get("light", []):
        if lc.get(CONF_ID) == config[CONF_LIGHT_ID]:
            light_conf = lc
    if light_conf is None:
        return config
    names = []
    for eff in light_conf.get("effects", []):
        for body in eff.values():
            if isinstance(body, dict) and "name" in body:
                names.append(str(body["name"]))
    known = {n.lower() for n in names}
    for i, eff in enumerate(config[CONF_EFFECTS]):
        if eff[CONF_EFFECT].lower() not in known:
            raise cv.Invalid(
                f"light '{config[CONF_LIGHT_ID]}' has no effect '{eff[CONF_EFFECT]}' "
                f"(its effects: {', '.join(names) or 'none'})",
                path=[CONF_EFFECTS, i, CONF_EFFECT],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_info_port(config[CONF_INFO_PORT]))
    if CONF_NODE_TYPE in config:
        cg.add(var.set_node_type(config[CONF_NODE_TYPE]))
    cg.add(var.set_discovery(config[CONF_DISCOVERY]))
    cg.add(var.set_sysinfo_interval(config[CONF_SYSINFO_INTERVAL]))
    cg.add(var.set_node_timeout(config[CONF_NODE_TIMEOUT]))
    cg.add(var.set_max_nodes(config[CONF_MAX_NODES]))
    cg.add(var.set_sync_enable(config[CONF_SYNC_ENABLE]))
    cg.add(var.set_sync_send(config[CONF_SYNC_SEND]))
    cg.add(var.set_sync_receive(config[CONF_SYNC_RECEIVE]))
    cg.add(var.set_receive_brightness(config[CONF_RECEIVE_BRIGHTNESS]))
    cg.add(var.set_receive_effects(config[CONF_RECEIVE_EFFECTS]))
    cg.add(var.set_send_on_change(config[CONF_SEND_ON_CHANGE]))
    cg.add(var.set_retransmit(config[CONF_RETRANSMIT]))
    cg.add(var.set_send_groups(config[CONF_SEND_GROUPS]))
    cg.add(var.set_receive_groups(config[CONF_RECEIVE_GROUPS]))

    if CONF_LIGHT_ID in config:
        light = await cg.get_variable(config[CONF_LIGHT_ID])
        cg.add(var.set_light(light))
    for eff in config[CONF_EFFECTS]:
        ids = cg.ArrayInitializer(*eff[CONF_WLED])
        cg.add(var.add_effect(eff[CONF_EFFECT], cg.RawExpression(f"std::vector<uint8_t>{ids}")))

    if config[CONF_HTTP_API]:
        cg.add_define("USE_WLED_EMU_HTTP")
        if CORE.is_esp32:
            # /ws: the WLED apps get online status and RSSI only over it.
            from esphome.components.esp32 import add_idf_sdkconfig_option

            add_idf_sdkconfig_option("CONFIG_HTTPD_WS_SUPPORT", True)
            add_idf_sdkconfig_option("CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT", True)
        cg.add(cg.RawExpression(f"{wled_emu_ns}::WledHttpHandler::install({var})"))

    if CORE.is_esp8266:
        cg.add_library("ESP8266WiFi", None)

    for conf in config.get(CONF_ON_GROUPS_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.uint8, "send"), (cg.uint8, "recv")], conf
        )
    for conf in config.get(CONF_ON_NODE_SEEN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "ip"), (cg.std_string, "name")], conf
        )


# ── actions ────────────────────────────────────────────────────────────

SEND_NOTIFIER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WledEmu),
        cv.Optional(CONF_ON, default=True): cv.templatable(cv.boolean),
        cv.Optional(CONF_BRIGHTNESS, default=255): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_RED, default=255): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_GREEN, default=255): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_BLUE, default=255): cv.templatable(cv.uint8_t),
        # ESPHome effect name (its first WLED ID from `effects:`) …
        cv.Exclusive(CONF_EFFECT, "effect"): cv.templatable(cv.string),
        # … or a WLED effect ID directly (see WLED_EFFECTS.md). Default 0 = Solid.
        cv.Exclusive(CONF_WLED_EFFECT, "effect"): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_GROUPS): cv.templatable(cv.uint8_t),
    }
)


@automation.register_action(
    "wled_emu.send_notifier", SendNotifierAction, SEND_NOTIFIER_SCHEMA
)
async def send_notifier_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    for key, setter, typ in (
        (CONF_ON, "set_on", cg.bool_),
        (CONF_BRIGHTNESS, "set_brightness", cg.uint8),
        (CONF_RED, "set_red", cg.uint8),
        (CONF_GREEN, "set_green", cg.uint8),
        (CONF_BLUE, "set_blue", cg.uint8),
        (CONF_EFFECT, "set_effect", cg.std_string),
        (CONF_WLED_EFFECT, "set_wled_effect", cg.uint8),
        (CONF_GROUPS, "set_groups", cg.uint8),
    ):
        if key in config:
            tmpl = await cg.templatable(config[key], args, typ)
            cg.add(getattr(var, setter)(tmpl))
    return var


@automation.register_action(
    "wled_emu.send_state",
    SendStateAction,
    cv.Schema({cv.GenerateID(): cv.use_id(WledEmu)}),
)
async def send_state_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
