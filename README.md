**English** | [Русский](README.ru.md) | [Українська](README.uk.md) | [Polski](README.pl.md)

# esphome-wled

ESPHome external component `wled_emu`: an ESPHome device behaves like a
**WLED** node on the local network. It follows other WLED nodes and drives
them in turn, shows up in their node lists and in the WLED apps, and answers
the WLED HTTP / JSON API. Any ESPHome light can be the "LED strip":
monochrome PWM, RGB, …

| Feature | Description |
|---|---|
| UDP sync (port 21324) | Receive and send brightness, colour and effect; 8 sync groups; a repeat with the followUp flag after 250 ms; nothing is echoed back within 500 ms. Packets carry compatibilityVersion 10, so peers with "Receive Segment bounds" keep their segments |
| Realtime | WARLS / DRGB / DRGBW / DNRGB / DNRGBW with a timeout, TPM2.NET (with the ping reply). When realtime ends, the previous state (on, brightness, colour, effect) comes back |
| Discovery | Node-info broadcast (port 65506) every 10 s; a table of the WLED peers heard, with a 60 s timeout |
| HTTP API (`http_api: true`) | `/json`, `/json/state` (GET + POST), `/json/info`, `/json/si`, `/json/nodes`, `/json/effects`, `/json/palettes`, `/json/cfg`, `/json/sync`, `/win&T=..&A=..`; the `/sync` (group matrix) and `/nodes` pages |
| WebSocket `/ws` (ESP32) | Pushes `{"state","info"}` on connect, on every change and every 30 s; accepts the same JSON as POST `/json/state`. The WLED app (WLED Native) shows a node as online, with its Wi-Fi signal, only over this connection |
| Entities | [switches](#switches) for the settings and sync groups (saved), [sensors, text sensors, numbers](#sensors-text-sensors-numbers) for peers, packets, the last packet, realtime and the UDP ports — no lambdas needed |
| Platforms | ESP8266 (Arduino) and ESP32 (Arduino / esp-idf) |

Without `light_id` the component only sends (a controller / remote): the sync
port is not opened, `send_notifier` and discovery work. With `light_id` it
applies incoming sync / realtime to the light and broadcasts the light's own
changes (brightness, colour, effect; checked every 250 ms).

## Configuration

```yaml
external_components:
  - source: github://kernelorg/esphome-wled@main
    components:
      - wled_emu
  # or a local copy:
  # - source:
  #     type: local
  #     path: <path>/esphome-wled/components
  #   components:
  #     - wled_emu

wled_emu:
  id: wled
  light_id: lamp             # the light that follows WLED; omit for send-only
  http_api: true             # needs web_server (and light_id)
  sync_send: true
  sync_receive: true
  send_groups: 0x01
  receive_groups: 0x01
  effects:                   # light effect <-> WLED effect IDs (WLED_EFFECTS.md)
    - effect: "Pulse"
      wled:
        - 2                    # Breathe: sent for "Pulse"
        - 12                   # Fade: also plays "Pulse" when received
    - effect: "Strobe"
      wled:
        - 23                   # Strobe
  on_node_seen:
    - logger.log:
        format: "new WLED peer %s (%s)"
        args:
          - 'ip.c_str()'
          - 'name.c_str()'
  on_groups_change:          # e.g. changed on the /sync page
    - logger.log:
        format: "groups: send 0x%02X, receive 0x%02X"
        args:
          - 'send'
          - 'recv'

# For the WLED apps: discovery over mDNS, the HTTP API on port 80.
mdns:
  services:
    - service: "_wled"
      protocol: "_tcp"
      port: 80
web_server:
  port: 80
```

| Option | Default | |
|---|---|---|
| `light_id` | — | light driven by WLED sync / realtime and broadcast on change |
| `http_api` | `false` | WLED HTTP / JSON API and `/ws`; needs `web_server` and `light_id` |
| `port` | `21324` | UDP sync / realtime port |
| `info_port` | `65506` | node-info (discovery) port; WLED before v16 used `port + 1` |
| `sync_enable` | `true` | master switch for sending and receiving |
| `sync_send` / `sync_receive` | `false` / `true` | send own changes / apply received packets |
| `receive_brightness` / `receive_effects` | `true` / `true` | which parts of a received packet are applied |
| `send_on_change` | `true` | broadcast the light's changes automatically |
| `retransmit` | `true` | repeat every notifier once (followUp) after 250 ms |
| `send_groups` / `receive_groups` | `0x01` / `0x01` | sync group masks, bit 0 = group 1 |
| `effects` | — | list of `effect:` (a light effect name) + `wled:` (one or more WLED effect IDs), see [Effects](#effects) |
| `discovery` | `true` | listen for node-info and keep the peer table |
| `sysinfo_interval` | `10s` | node-info broadcast interval |
| `node_timeout` | `60s` | a silent peer is dropped after this time |
| `max_nodes` | `16` | peer table size |
| `node_type` | auto | type reported to peers (32 = ESP32, 210 = ESP8266) |

## Effects

WLED sync packets carry the effect as a WLED effect ID (Breathe = 2,
Strobe = 23, …); ESPHome lights have named effects. The `effects:` list says
which WLED effects each light effect stands for:

- **received:** any listed ID plays that light effect; an ID that is not
  listed (or 0, Solid) plays no effect;
- **sent:** the light effect goes out as its **first** ID; a light effect
  that is not listed goes out as 0 (Solid);
- several light effects may share an ID — the first entry wins on receive;
- the names are checked against the light's effects when the config is
  validated.

All WLED IDs and which of them work on a single LED (monochrome lamp):
[WLED_EFFECTS.md](WLED_EFFECTS.md). The HTTP API and the WLED app list all
of the light's effects (mapped or not) and switch them directly.

## Actions

```yaml
- wled_emu.send_notifier:      # to the send groups, or `groups:`
    "on": true                 # quoted: a bare `on` is a YAML boolean
    brightness: 200
    red: 255
    green: 0
    blue: 0
    effect: "Pulse"            # light effect → its first WLED ID from `effects:`
    # wled_effect: 2           # … or a WLED effect ID directly (default 0 = Solid)
    groups: 0x03
- wled_emu.send_state:         # the bound light's current state
```

All fields are templatable (`!lambda`). Example — a doorbell button makes every
lamp in groups 1 and 2 flash:

```yaml
binary_sensor:
  - platform: gpio
    name: "Doorbell"
    pin:
      number: GPIO0
      inverted: true
      mode:
        input: true
        pullup: true
    on_press:
      - wled_emu.send_notifier:
          brightness: 255
          wled_effect: 23
          groups: 0x03
      - delay: 5s
      - wled_emu.send_notifier:
          "on": false
          groups: 0x03
```

## Switches

The runtime settings and the sync groups as switches — for Home Assistant or
the web page, no lambdas needed:

```yaml
switch:
  - platform: wled_emu
    type: sync
    name: "Sync"
  - platform: wled_emu
    type: sync_send
    name: "Sync send"
  - platform: wled_emu
    type: sync_receive
    name: "Sync receive"
  - platform: wled_emu
    type: receive_group
    group: 2
    name: "Receive group 2"
```

| Option | |
|---|---|
| `type` | `sync`, `sync_send`, `sync_receive`, `receive_brightness`, `receive_effects`, `send_on_change`, `retransmit`, `send_group`, `receive_group` |
| `group` | `1`..`8`, for `send_group` / `receive_group` |
| `wled_emu_id` | the `wled_emu` to control (only with several) |
| `restore_mode` | default `RESTORE_DEFAULT_OFF`: the saved value is applied after a reboot; with nothing saved yet the value from the `wled_emu` config stays. `ALWAYS_ON` / `ALWAYS_OFF` force a value at boot |

Changes made elsewhere — the `/sync` page, `/json/sync`, the debug UI,
lambdas — show up in the switches and are saved with them.

## Sensors, text sensors, numbers

```yaml
sensor:
  - platform: wled_emu
    type: peers
    name: "WLED peers"
  - platform: wled_emu
    type: rx_packets
    name: "Packets received"
  - platform: wled_emu
    type: tx_packets
    name: "Packets sent"
binary_sensor:
  - platform: wled_emu
    type: realtime
    name: "Realtime"
text_sensor:
  - platform: wled_emu
    type: last_packet
    name: "Last WLED packet"
  - platform: wled_emu
    type: last_sender
    name: "Last sender"
  - platform: wled_emu
    type: peers
    name: "WLED peer list"
number:
  - platform: wled_emu
    type: port
    name: "UDP port"
    mode: box
  - platform: wled_emu
    type: info_port
    name: "Discovery port"
    mode: box
```

| Platform | `type` | Value |
|---|---|---|
| `sensor` | `peers` | WLED nodes heard |
| | `rx_packets` / `tx_packets` | packets received on the sync port / broadcast (since boot) |
| `binary_sensor` | `realtime` | realtime data is driving the light |
| `text_sensor` | `last_packet` | e.g. `notifier applied: bri 200 rgb 255/0/0 fx 2 grp 01`, `DRGB applied: …`, `notifier other group: …` |
| | `last_sender` | IP of the last packet's sender |
| | `peers` | `name (ip), …` (up to 255 characters) |
| `number` | `port` / `info_port` | sync / discovery UDP port; saved (`restore_value`, default `true`), the socket is reopened at once |

The values are checked once a second and published only when they change.

## Triggers

| Trigger | Variables | |
|---|---|---|
| `on_node_seen` | `ip`, `name` (`std::string`) | a new WLED peer was heard |
| `on_groups_change` | `send`, `recv` (`uint8_t`) | the group masks changed (`/sync` page, `/json/sync`, lambda) |

## Lambda API

| Method | |
|---|---|
| `send_notifier(on, bri, r, g, b, wled_effect[, groups])`, `send_state()`, `send_sysinfo()` | send now (`wled_effect` = WLED effect ID) |
| `fxmode_for_effect(name)`, `effect_for_fxmode(id)` | look up the `effects:` table |
| `set_sync_enable/send/receive(bool)`, `set_receive_brightness/effects(bool)`, `set_send_on_change(bool)`, `set_retransmit(bool)` and the `get_…()` counterparts | runtime switches (not saved by themselves — use the [switches](#switches) to keep them) |
| `set_send_groups(m)`, `set_receive_groups(m)`, `set_group_bit(send, 1..8, on)`, `get_send_groups()`, `get_receive_groups()` | group masks |
| `set_port(p)`, `set_info_port(p)` | the sockets are reopened on the next loop |
| `nodes()` → `std::vector<WledNode>{ip, name, type, vid, last_seen}`, `node_count()` | peer table; `type & 0x80` = the peer's light is on |
| `rx_count()`, `tx_count()` | packets received on the sync port / broadcast |
| `last_rx_kind()`, `last_rx_result()`, `last_rx_detail()`, `last_rx_ip()`, `last_rx_age_ms()` | the last packet received, e.g. `"notifier"`, `"other group"`, `"bri 200 rgb 255/0/0 fx 2 grp 01"` |
| `in_realtime()` | realtime data is driving the light right now |
| `add_on_settings_change_callback(f)`, `add_on_groups_change_callback(f)` | called when a runtime switch / the group masks change |

## WLED apps

- The node is found over mDNS (`_wled._tcp`, see the configuration above);
  phone and node must be in one network, or the router must forward mDNS.
- **ESP32:** the WLED app connects to `/ws` and shows the node online with its
  Wi-Fi signal; on/off, brightness, colour and effect work from the app.
  When the node is opened, the app shows the device's own page — the ESPHome
  web server.
- **ESP8266:** no WebSocket yet, so the WLED app lists the node as offline.
  Sync with other nodes, the HTTP API and the other nodes' lists work.

## Examples

| File | |
|---|---|
| [`example/wled_mono_lamp_esp8266.yaml`](example/wled_mono_lamp_esp8266.yaml) | monochrome PWM lamp on a NodeMCU (D1, active low, 1 kHz): follows WLED sync and realtime, six effects, sync switches (switch platform) saved over a reboot, "pulse all lamps" button, status sensors, UDP ports |
| [`example/wled_emu_esp32.yaml`](example/wled_emu_esp32.yaml) | ESP32 remote without a light: buttons send to the groups, a sensor counts the peers |
| [`example/wled_lvgl_tdisplay.yaml`](example/wled_lvgl_tdisplay.yaml) | LilyGO T-Display debug firmware: virtual RGB lamp on the screen + debug UI, all entity types; Wi-Fi over Improv BLE |

Copy `example/secrets.yaml.example` to `example/secrets.yaml` and build from
the repository root:

```bash
docker run --rm -v "$PWD":/config -w /config ghcr.io/esphome/esphome:2026.10.1 \
  compile example/wled_mono_lamp_esp8266.yaml
```

## Optional on-device debug UI (LVGL)

[`ui/lvgl_debug.yaml`](ui/lvgl_debug.yaml) is a package for a small landscape
display (240×135) with two buttons (ESP32, esp-idf); it is not needed for
normal operation.

```yaml
packages:
  wled_ui: !include
    file: <path>/esphome-wled/ui/lvgl_debug.yaml
    vars:
      wled_id: wled
      light_id: lamp             # the light bound to wled_emu
      display_id: lcd
      rotation: "90"             # landscape rotation when not flipped
      key_left: btn_left         # binary_sensor: left button
      key_right: btn_right       # binary_sensor: right button
      backlight_id: backlight
      deep_sleep_id: deep_sleep_ctl
```

| Page | |
|---|---|
| Status | IP, Wi-Fi, sync switches, group masks, peers, counters, the last packet received (kind, result, values, sender); a round LED mirrors the light |
| Send | broadcast red / green / blue / white / pulse / strobe / off to the send groups, or the lamp's state |
| Lamp | change the light locally (sent on with "Send" sync on) |
| Net | Wi-Fi, IP, the peers heard; *Forget Wi-Fi* reboots into Improv |
| Settings | sync options, sync groups (2 × 8 buttons), flip the screen 180°, deep sleep after 30 s / 1 / 3 / 5 min or never |

Buttons: left = move the focus (red frame), right = press; with the screen
flipped they swap, so they keep matching what you see. Green = option on /
group member. Either button wakes the board from deep sleep.

## Limitations

- JSON and HTTP requests over UDP are not handled.
- Received effects play only through the `effects:` table; an RGB light gets
  the colour of the first LED of realtime data.
- The HTTP API lists the light's effects and one palette.
- On ESP32 the component uses 3 UDP sockets; `/ws` keeps up to 4 clients.
