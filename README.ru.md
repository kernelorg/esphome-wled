[English](README.md) | **Русский** | [Українська](README.uk.md) | [Polski](README.pl.md)

# esphome-wled

Внешний компонент ESPHome `wled_emu`: устройство на ESPHome ведёт себя в
локальной сети как узел **WLED**. Оно повторяет за другими узлами WLED и само
управляет ими, видно в их списках узлов и в приложениях WLED, отвечает на
HTTP / JSON API WLED. «Светодиодной лентой» может быть любой свет ESPHome:
монохромный ШИМ, RGB, …

| Возможность | Описание |
|---|---|
| UDP-синхронизация (порт 21324) | Приём и отправка яркости, цвета и эффекта; 8 групп синхронизации; повтор с флагом followUp через 250 мс; в течение 500 мс ничего не отражается обратно. Пакеты идут с compatibilityVersion 10, поэтому у узлов с «Receive Segment bounds» сегменты не сбрасываются |
| Realtime | WARLS / DRGB / DRGBW / DNRGB / DNRGBW с таймаутом, TPM2.NET (с ответом на ping). После окончания realtime возвращается прежнее состояние (вкл., яркость, цвет, эффект) |
| Обнаружение | Рассылка node-info (порт 65506) каждые 10 с; таблица услышанных узлов WLED с таймаутом 60 с |
| HTTP API (`http_api: true`) | `/json`, `/json/state` (GET + POST), `/json/info`, `/json/si`, `/json/nodes`, `/json/effects`, `/json/palettes`, `/json/cfg`, `/json/sync`, `/win&T=..&A=..`; страницы `/sync` (матрица групп) и `/nodes` |
| WebSocket `/ws` (ESP32) | Отправляет `{"state","info"}` при подключении, при каждом изменении и каждые 30 с; принимает тот же JSON, что POST `/json/state`. Приложение WLED (WLED Native) показывает узел «в сети» и с сигналом Wi-Fi только через это подключение |
| Сущности | [переключатели](#переключатели) для параметров и групп синхронизации (сохраняются), [датчики, текстовые датчики, числа](#датчики-текстовые-датчики-числа) для узлов, пакетов, последнего пакета, realtime и UDP-портов — без лямбд |
| Платформы | ESP8266 (Arduino) и ESP32 (Arduino / esp-idf) |

Без `light_id` компонент только отправляет (пульт / контроллер): порт
синхронизации не открывается, `send_notifier` и обнаружение работают. С
`light_id` он применяет входящую синхронизацию / realtime к свету и
рассылает изменения самого света (яркость, цвет, эффект; проверка каждые
250 мс).

## Конфигурация

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
  light_id: lamp             # свет, который повторяет WLED; без него — только отправка
  http_api: true             # нужен web_server (и light_id)
  sync_send: true
  sync_receive: true
  send_groups: 0x01
  receive_groups: 0x01
  effects:                   # эффект света <-> номера эффектов WLED (WLED_EFFECTS.ru.md)
    - effect: "Pulse"
      wled:
        - 2                    # Breathe: отправляется для «Pulse»
        - 12                   # Fade: при приёме тоже включает «Pulse»
    - effect: "Strobe"
      wled:
        - 23                   # Strobe
  on_node_seen:
    - logger.log:
        format: "new WLED peer %s (%s)"
        args:
          - 'ip.c_str()'
          - 'name.c_str()'
  on_groups_change:          # например, изменены на странице /sync
    - logger.log:
        format: "groups: send 0x%02X, receive 0x%02X"
        args:
          - 'send'
          - 'recv'

# Для приложений WLED: обнаружение по mDNS, HTTP API на порту 80.
mdns:
  services:
    - service: "_wled"
      protocol: "_tcp"
      port: 80
web_server:
  port: 80
```

| Параметр | По умолчанию | |
|---|---|---|
| `light_id` | — | свет, которым управляют синхронизация / realtime WLED и который рассылается при изменении |
| `http_api` | `false` | HTTP / JSON API WLED и `/ws`; нужны `web_server` и `light_id` |
| `port` | `21324` | UDP-порт синхронизации / realtime |
| `info_port` | `65506` | порт node-info (обнаружение); WLED до v16 использовал `port + 1` |
| `sync_enable` | `true` | главный выключатель отправки и приёма |
| `sync_send` / `sync_receive` | `false` / `true` | отправлять свои изменения / применять принятые пакеты |
| `receive_brightness` / `receive_effects` | `true` / `true` | какие части принятого пакета применяются |
| `send_on_change` | `true` | автоматически рассылать изменения света |
| `retransmit` | `true` | повторять каждый пакет один раз (followUp) через 250 мс |
| `send_groups` / `receive_groups` | `0x01` / `0x01` | маски групп синхронизации, бит 0 = группа 1 |
| `effects` | — | список из `effect:` (имя эффекта света) + `wled:` (один или несколько номеров эффектов WLED), см. [Эффекты](#эффекты) |
| `discovery` | `true` | слушать node-info и вести таблицу узлов |
| `sysinfo_interval` | `10s` | интервал рассылки node-info |
| `node_timeout` | `60s` | молчащий узел удаляется через это время |
| `max_nodes` | `16` | размер таблицы узлов |
| `node_type` | авто | тип, сообщаемый узлам (32 = ESP32, 210 = ESP8266) |

## Эффекты

В пакетах синхронизации WLED эффект передаётся номером эффекта WLED
(Breathe = 2, Strobe = 23, …), а у света ESPHome эффекты именованные. Список
`effects:` задаёт, каким эффектам WLED соответствует каждый эффект света:

- **приём:** любой из перечисленных номеров включает этот эффект света;
  номер, которого нет в списке (или 0, Solid), выключает эффект;
- **отправка:** эффект света уходит своим **первым** номером; эффект,
  которого нет в списке, уходит как 0 (Solid);
- несколько эффектов света могут делить один номер — при приёме срабатывает
  первый по списку;
- имена проверяются по эффектам света при проверке конфигурации.

Все номера WLED и какие из них работают на одном светодиоде (монохромная
лампа): [WLED_EFFECTS.ru.md](WLED_EFFECTS.ru.md). HTTP API и приложение WLED
показывают все эффекты света (с сопоставлением и без) и переключают их
напрямую.

## Действия

```yaml
- wled_emu.send_notifier:      # в группы отправки или в `groups:`
    "on": true                 # в кавычках: голое `on` в YAML — логическое значение
    brightness: 200
    red: 255
    green: 0
    blue: 0
    effect: "Pulse"            # эффект света → его первый номер WLED из `effects:`
    # wled_effect: 2           # … или номер эффекта WLED напрямую (по умолчанию 0 = Solid)
    groups: 0x03
- wled_emu.send_state:         # текущее состояние привязанного света
```

Все поля можно задавать шаблоном (`!lambda`). Пример — кнопка звонка
заставляет мигать все лампы групп 1 и 2:

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

## Переключатели

Параметры работы и группы синхронизации в виде переключателей — для Home
Assistant или веб-страницы, без лямбд:

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

| Параметр | |
|---|---|
| `type` | `sync`, `sync_send`, `sync_receive`, `receive_brightness`, `receive_effects`, `send_on_change`, `retransmit`, `send_group`, `receive_group` |
| `group` | `1`..`8`, для `send_group` / `receive_group` |
| `wled_emu_id` | какой `wled_emu` переключать (только если их несколько) |
| `restore_mode` | по умолчанию `RESTORE_DEFAULT_OFF`: после перезагрузки применяется сохранённое значение; пока ничего не сохранено, остаётся значение из конфига `wled_emu`. `ALWAYS_ON` / `ALWAYS_OFF` задают значение при загрузке принудительно |

Изменения, сделанные в другом месте — на странице `/sync`, через
`/json/sync`, в отладочном интерфейсе, из лямбд, — отображаются на
переключателях и сохраняются вместе с ними.

## Датчики, текстовые датчики, числа

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

| Платформа | `type` | Значение |
|---|---|---|
| `sensor` | `peers` | услышанные узлы WLED |
| | `rx_packets` / `tx_packets` | пакетов принято на порт синхронизации / разослано (с загрузки) |
| `binary_sensor` | `realtime` | светом управляют данные realtime |
| `text_sensor` | `last_packet` | например `notifier applied: bri 200 rgb 255/0/0 fx 2 grp 01`, `DRGB applied: …`, `notifier other group: …` |
| | `last_sender` | IP отправителя последнего пакета |
| | `peers` | `имя (ip), …` (до 255 символов) |
| `number` | `port` / `info_port` | UDP-порт синхронизации / обнаружения; сохраняется (`restore_value`, по умолчанию `true`), сокет переоткрывается сразу |

Значения проверяются раз в секунду и публикуются только при изменении.

## Триггеры

| Триггер | Переменные | |
|---|---|---|
| `on_node_seen` | `ip`, `name` (`std::string`) | услышан новый узел WLED |
| `on_groups_change` | `send`, `recv` (`uint8_t`) | изменились маски групп (страница `/sync`, `/json/sync`, лямбда) |

## API из лямбд

| Метод | |
|---|---|
| `send_notifier(on, bri, r, g, b, wled_effect[, groups])`, `send_state()`, `send_sysinfo()` | отправить сейчас (`wled_effect` — номер эффекта WLED) |
| `fxmode_for_effect(name)`, `effect_for_fxmode(id)` | поиск по таблице `effects:` |
| `set_sync_enable/send/receive(bool)`, `set_receive_brightness/effects(bool)`, `set_send_on_change(bool)`, `set_retransmit(bool)` и парные `get_…()` | переключатели во время работы (сами не сохраняются — для этого есть [переключатели](#переключатели)) |
| `set_send_groups(m)`, `set_receive_groups(m)`, `set_group_bit(send, 1..8, on)`, `get_send_groups()`, `get_receive_groups()` | маски групп |
| `set_port(p)`, `set_info_port(p)` | сокеты переоткрываются на следующем цикле |
| `nodes()` → `std::vector<WledNode>{ip, name, type, vid, last_seen}`, `node_count()` | таблица узлов; `type & 0x80` = свет узла включён |
| `rx_count()`, `tx_count()` | пакетов принято на порт синхронизации / разослано |
| `last_rx_kind()`, `last_rx_result()`, `last_rx_detail()`, `last_rx_ip()`, `last_rx_age_ms()` | последний принятый пакет, например `"notifier"`, `"other group"`, `"bri 200 rgb 255/0/0 fx 2 grp 01"` |
| `in_realtime()` | сейчас светом управляют данные realtime |
| `add_on_settings_change_callback(f)`, `add_on_groups_change_callback(f)` | вызываются при изменении переключателя работы / масок групп |

## Приложения WLED

- Узел находится по mDNS (`_wled._tcp`, см. конфигурацию выше); телефон и
  узел должны быть в одной сети, либо роутер должен пересылать mDNS.
- **ESP32:** приложение WLED подключается к `/ws` и показывает узел «в сети»
  с сигналом Wi-Fi; из приложения работают вкл./выкл., яркость, цвет и
  эффект. При открытии узла приложение показывает собственную страницу
  устройства — веб-сервер ESPHome.
- **ESP8266:** WebSocket пока нет, поэтому в приложении WLED узел «не в
  сети». Синхронизация с другими узлами, HTTP API и списки других узлов
  работают.

## Примеры

| Файл | |
|---|---|
| [`example/wled_mono_lamp_esp8266.yaml`](example/wled_mono_lamp_esp8266.yaml) | монохромная ШИМ-лампа на NodeMCU (D1, активный низкий, 1 кГц): повторяет синхронизацию и realtime WLED, шесть эффектов, переключатели синхронизации (платформа switch) сохраняются после перезагрузки, кнопка «пульс всем лампам», датчики состояния, UDP-порты |
| [`example/wled_emu_esp32.yaml`](example/wled_emu_esp32.yaml) | пульт на ESP32 без света: кнопки отправляют в группы, датчик считает узлы |
| [`example/wled_lvgl_tdisplay.yaml`](example/wled_lvgl_tdisplay.yaml) | отладочная прошивка LilyGO T-Display: виртуальная RGB-лампа на экране + отладочный интерфейс, все типы сущностей; Wi-Fi через Improv BLE |

Скопируйте `example/secrets.yaml.example` в `example/secrets.yaml` и
соберите из корня репозитория:

```bash
docker run --rm -v "$PWD":/config -w /config ghcr.io/esphome/esphome:2026.10.1 \
  compile example/wled_mono_lamp_esp8266.yaml
```

## Необязательный отладочный интерфейс (LVGL)

[`ui/lvgl_debug.yaml`](ui/lvgl_debug.yaml) — пакет для небольшого
горизонтального дисплея (240×135) с двумя кнопками (ESP32, esp-idf); для
обычной работы он не нужен.

```yaml
packages:
  wled_ui: !include
    file: <path>/esphome-wled/ui/lvgl_debug.yaml
    vars:
      wled_id: wled
      light_id: lamp             # свет, привязанный к wled_emu
      display_id: lcd
      rotation: "90"             # горизонтальный поворот без переворота
      key_left: btn_left         # binary_sensor: левая кнопка
      key_right: btn_right       # binary_sensor: правая кнопка
      backlight_id: backlight
      deep_sleep_id: deep_sleep_ctl
```

| Страница | |
|---|---|
| Status | IP, Wi-Fi, переключатели синхронизации, маски групп, узлы, счётчики, последний принятый пакет (тип, результат, значения, отправитель); круглый светодиод повторяет свет |
| Send | разослать в группы отправки красный / зелёный / синий / белый / пульс / строб / выкл. или состояние лампы |
| Lamp | изменить свет локально (уходит в сеть при включённой синхронизации «Send») |
| Net | Wi-Fi, IP, услышанные узлы; *Forget Wi-Fi* перезагружает в Improv |
| Settings | параметры синхронизации, группы (2 × 8 кнопок), переворот экрана на 180°, deep sleep через 30 с / 1 / 3 / 5 мин или никогда |

Кнопки: левая — перевести фокус (красная рамка), правая — нажать; при
перевёрнутом экране они меняются местами и остаются «левой» и «правой» для
вас. Зелёный = параметр включён / группа выбрана. Любая кнопка будит плату
из deep sleep.

## Ограничения

- JSON и HTTP-запросы по UDP не обрабатываются.
- Принятые эффекты включаются только через таблицу `effects:`; RGB-свет
  получает цвет первого светодиода из данных realtime.
- HTTP API показывает эффекты света и одну палитру.
- На ESP32 компонент использует 3 UDP-сокета; `/ws` держит до 4 клиентов.
