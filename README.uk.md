[English](README.md) | [Русский](README.ru.md) | **Українська** | [Polski](README.pl.md)

# esphome-wled

Зовнішній компонент ESPHome `wled_emu`: пристрій на ESPHome поводиться в
локальній мережі як вузол **WLED**. Він повторює за іншими вузлами WLED і
сам керує ними, видимий у їхніх списках вузлів і в застосунках WLED,
відповідає на HTTP / JSON API WLED. «Світлодіодною стрічкою» може бути
будь-яке світло ESPHome: монохромний ШІМ, RGB, …

| Можливість | Опис |
|---|---|
| UDP-синхронізація (порт 21324) | Приймання й надсилання яскравості, кольору та ефекту; 8 груп синхронізації; повтор із прапорцем followUp через 250 мс; протягом 500 мс нічого не відбивається назад. Пакети йдуть із compatibilityVersion 10, тому у вузлів із «Receive Segment bounds» сегменти не скидаються |
| Realtime | WARLS / DRGB / DRGBW / DNRGB / DNRGBW з таймаутом, TPM2.NET (з відповіддю на ping). Після завершення realtime повертається попередній стан (увімк., яскравість, колір, ефект) |
| Виявлення | Розсилання node-info (порт 65506) кожні 10 с; таблиця почутих вузлів WLED з таймаутом 60 с |
| HTTP API (`http_api: true`) | `/json`, `/json/state` (GET + POST), `/json/info`, `/json/si`, `/json/nodes`, `/json/effects`, `/json/palettes`, `/json/cfg`, `/json/sync`, `/win&T=..&A=..`; сторінки `/sync` (матриця груп) і `/nodes` |
| WebSocket `/ws` (ESP32) | Надсилає `{"state","info"}` під час підключення, за кожної зміни й кожні 30 с; приймає той самий JSON, що POST `/json/state`. Застосунок WLED (WLED Native) показує вузол «у мережі» та із сигналом Wi-Fi лише через це підключення |
| Сутності | [перемикачі](#перемикачі) для параметрів і груп синхронізації (зберігаються), [датчики, текстові датчики, числа](#датчики-текстові-датчики-числа) для вузлів, пакетів, останнього пакета, realtime і UDP-портів — без лямбд |
| Платформи | ESP8266 (Arduino) і ESP32 (Arduino / esp-idf) |

Без `light_id` компонент лише надсилає (пульт / контролер): порт
синхронізації не відкривається, `send_notifier` і виявлення працюють. З
`light_id` він застосовує вхідну синхронізацію / realtime до світла й
розсилає зміни самого світла (яскравість, колір, ефект; перевірка кожні
250 мс).

## Конфігурація

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
  light_id: lamp             # світло, що повторює WLED; без нього — лише надсилання
  http_api: true             # потрібен web_server (і light_id)
  sync_send: true
  sync_receive: true
  send_groups: 0x01
  receive_groups: 0x01
  effects:                   # ефект світла <-> номери ефектів WLED (WLED_EFFECTS.uk.md)
    - effect: "Pulse"
      wled:
        - 2                    # Breathe: надсилається для «Pulse»
        - 12                   # Fade: під час приймання теж вмикає «Pulse»
    - effect: "Strobe"
      wled:
        - 23                   # Strobe
  on_node_seen:
    - logger.log:
        format: "new WLED peer %s (%s)"
        args:
          - 'ip.c_str()'
          - 'name.c_str()'
  on_groups_change:          # наприклад, змінені на сторінці /sync
    - logger.log:
        format: "groups: send 0x%02X, receive 0x%02X"
        args:
          - 'send'
          - 'recv'

# Для застосунків WLED: виявлення через mDNS, HTTP API на порту 80.
mdns:
  services:
    - service: "_wled"
      protocol: "_tcp"
      port: 80
web_server:
  port: 80
```

| Параметр | За замовчуванням | |
|---|---|---|
| `light_id` | — | світло, яким керують синхронізація / realtime WLED і яке розсилається під час зміни |
| `http_api` | `false` | HTTP / JSON API WLED і `/ws`; потрібні `web_server` і `light_id` |
| `port` | `21324` | UDP-порт синхронізації / realtime |
| `info_port` | `65506` | порт node-info (виявлення); WLED до v16 використовував `port + 1` |
| `sync_enable` | `true` | головний вимикач надсилання й приймання |
| `sync_send` / `sync_receive` | `false` / `true` | надсилати свої зміни / застосовувати прийняті пакети |
| `receive_brightness` / `receive_effects` | `true` / `true` | які частини прийнятого пакета застосовуються |
| `send_on_change` | `true` | автоматично розсилати зміни світла |
| `retransmit` | `true` | повторювати кожен пакет один раз (followUp) через 250 мс |
| `send_groups` / `receive_groups` | `0x01` / `0x01` | маски груп синхронізації, біт 0 = група 1 |
| `effects` | — | список із `effect:` (назва ефекту світла) + `wled:` (один або кілька номерів ефектів WLED), див. [Ефекти](#ефекти) |
| `discovery` | `true` | слухати node-info і вести таблицю вузлів |
| `sysinfo_interval` | `10s` | інтервал розсилання node-info |
| `node_timeout` | `60s` | вузол, що мовчить, видаляється через цей час |
| `max_nodes` | `16` | розмір таблиці вузлів |
| `node_type` | авто | тип, що повідомляється вузлам (32 = ESP32, 210 = ESP8266) |

## Ефекти

У пакетах синхронізації WLED ефект передається номером ефекту WLED
(Breathe = 2, Strobe = 23, …), а у світла ESPHome ефекти іменовані. Список
`effects:` задає, яким ефектам WLED відповідає кожен ефект світла:

- **приймання:** будь-який із перелічених номерів вмикає цей ефект світла;
  номер, якого немає в списку (або 0, Solid), вимикає ефект;
- **надсилання:** ефект світла йде своїм **першим** номером; ефект, якого
  немає в списку, йде як 0 (Solid);
- кілька ефектів світла можуть ділити один номер — під час приймання
  спрацьовує перший за списком;
- назви перевіряються за ефектами світла під час перевірки конфігурації.

Усі номери WLED і які з них працюють на одному світлодіоді (монохромна
лампа): [WLED_EFFECTS.uk.md](WLED_EFFECTS.uk.md). HTTP API і застосунок WLED
показують усі ефекти світла (із зіставленням і без) і перемикають їх
напряму.

## Дії

```yaml
- wled_emu.send_notifier:      # у групи надсилання або в `groups:`
    "on": true                 # у лапках: голе `on` у YAML — логічне значення
    brightness: 200
    red: 255
    green: 0
    blue: 0
    effect: "Pulse"            # ефект світла → його перший номер WLED з `effects:`
    # wled_effect: 2           # … або номер ефекту WLED напряму (за замовчуванням 0 = Solid)
    groups: 0x03
- wled_emu.send_state:         # поточний стан прив'язаного світла
```

Усі поля можна задавати шаблоном (`!lambda`). Приклад — кнопка дзвінка
змушує блимати всі лампи груп 1 і 2:

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

## Перемикачі

Параметри роботи та групи синхронізації у вигляді перемикачів — для Home
Assistant або вебсторінки, без лямбд:

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
| `wled_emu_id` | який `wled_emu` перемикати (лише якщо їх кілька) |
| `restore_mode` | за замовчуванням `RESTORE_DEFAULT_OFF`: після перезавантаження застосовується збережене значення; поки нічого не збережено, залишається значення з конфігу `wled_emu`. `ALWAYS_ON` / `ALWAYS_OFF` задають значення під час завантаження примусово |

Зміни, зроблені деінде — на сторінці `/sync`, через `/json/sync`, у
налагоджувальному інтерфейсі, з лямбд, — відображаються на перемикачах і
зберігаються разом із ними.

## Датчики, текстові датчики, числа

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

| Платформа | `type` | Значення |
|---|---|---|
| `sensor` | `peers` | почуті вузли WLED |
| | `rx_packets` / `tx_packets` | пакетів прийнято на порт синхронізації / розіслано (від завантаження) |
| `binary_sensor` | `realtime` | світлом керують дані realtime |
| `text_sensor` | `last_packet` | наприклад `notifier applied: bri 200 rgb 255/0/0 fx 2 grp 01`, `DRGB applied: …`, `notifier other group: …` |
| | `last_sender` | IP відправника останнього пакета |
| | `peers` | `ім'я (ip), …` (до 255 символів) |
| `number` | `port` / `info_port` | UDP-порт синхронізації / виявлення; зберігається (`restore_value`, за замовчуванням `true`), сокет перевідкривається одразу |

Значення перевіряються раз на секунду й публікуються лише в разі зміни.

## Тригери

| Тригер | Змінні | |
|---|---|---|
| `on_node_seen` | `ip`, `name` (`std::string`) | почуто новий вузол WLED |
| `on_groups_change` | `send`, `recv` (`uint8_t`) | змінилися маски груп (сторінка `/sync`, `/json/sync`, лямбда) |

## API з лямбд

| Метод | |
|---|---|
| `send_notifier(on, bri, r, g, b, wled_effect[, groups])`, `send_state()`, `send_sysinfo()` | надіслати зараз (`wled_effect` — номер ефекту WLED) |
| `fxmode_for_effect(name)`, `effect_for_fxmode(id)` | пошук у таблиці `effects:` |
| `set_sync_enable/send/receive(bool)`, `set_receive_brightness/effects(bool)`, `set_send_on_change(bool)`, `set_retransmit(bool)` і парні `get_…()` | перемикачі під час роботи (самі не зберігаються — для цього є [перемикачі](#перемикачі)) |
| `set_send_groups(m)`, `set_receive_groups(m)`, `set_group_bit(send, 1..8, on)`, `get_send_groups()`, `get_receive_groups()` | маски груп |
| `set_port(p)`, `set_info_port(p)` | сокети перевідкриваються на наступному циклі |
| `nodes()` → `std::vector<WledNode>{ip, name, type, vid, last_seen}`, `node_count()` | таблиця вузлів; `type & 0x80` = світло вузла увімкнене |
| `rx_count()`, `tx_count()` | пакетів прийнято на порт синхронізації / розіслано |
| `last_rx_kind()`, `last_rx_result()`, `last_rx_detail()`, `last_rx_ip()`, `last_rx_age_ms()` | останній прийнятий пакет, наприклад `"notifier"`, `"other group"`, `"bri 200 rgb 255/0/0 fx 2 grp 01"` |
| `in_realtime()` | зараз світлом керують дані realtime |
| `add_on_settings_change_callback(f)`, `add_on_groups_change_callback(f)` | викликаються в разі зміни перемикача роботи / масок груп |

## Застосунки WLED

- Вузол знаходиться через mDNS (`_wled._tcp`, див. конфігурацію вище);
  телефон і вузол мають бути в одній мережі, або роутер має пересилати mDNS.
- **ESP32:** застосунок WLED підключається до `/ws` і показує вузол «у
  мережі» із сигналом Wi-Fi; із застосунку працюють увімк./вимк.,
  яскравість, колір і ефект. Під час відкриття вузла застосунок показує
  власну сторінку пристрою — вебсервер ESPHome.
- **ESP8266:** WebSocket поки немає, тому в застосунку WLED вузол «не в
  мережі». Синхронізація з іншими вузлами, HTTP API і списки інших вузлів
  працюють.

## Приклади

| Файл | |
|---|---|
| [`example/wled_mono_lamp_esp8266.yaml`](example/wled_mono_lamp_esp8266.yaml) | монохромна ШІМ-лампа на NodeMCU (D1, активний низький, 1 кГц): повторює синхронізацію та realtime WLED, шість ефектів, перемикачі синхронізації (платформа switch) зберігаються після перезавантаження, кнопка «пульс усім лампам», датчики стану, UDP-порти |
| [`example/wled_emu_esp32.yaml`](example/wled_emu_esp32.yaml) | пульт на ESP32 без світла: кнопки надсилають у групи, датчик рахує вузли |
| [`example/wled_lvgl_tdisplay.yaml`](example/wled_lvgl_tdisplay.yaml) | налагоджувальна прошивка LilyGO T-Display: віртуальна RGB-лампа на екрані + налагоджувальний інтерфейс, усі типи сутностей; Wi-Fi через Improv BLE |

Скопіюйте `example/secrets.yaml.example` в `example/secrets.yaml` і
зберіть із кореня репозиторію:

```bash
docker run --rm -v "$PWD":/config -w /config ghcr.io/esphome/esphome:2026.10.1 \
  compile example/wled_mono_lamp_esp8266.yaml
```

## Необов'язковий налагоджувальний інтерфейс (LVGL)

[`ui/lvgl_debug.yaml`](ui/lvgl_debug.yaml) — пакет для невеликого
горизонтального дисплея (240×135) з двома кнопками (ESP32, esp-idf); для
звичайної роботи він не потрібен.

```yaml
packages:
  wled_ui: !include
    file: <path>/esphome-wled/ui/lvgl_debug.yaml
    vars:
      wled_id: wled
      light_id: lamp             # світло, прив'язане до wled_emu
      display_id: lcd
      rotation: "90"             # горизонтальний поворот без перевертання
      key_left: btn_left         # binary_sensor: ліва кнопка
      key_right: btn_right       # binary_sensor: права кнопка
      backlight_id: backlight
      deep_sleep_id: deep_sleep_ctl
```

| Сторінка | |
|---|---|
| Status | IP, Wi-Fi, перемикачі синхронізації, маски груп, вузли, лічильники, останній прийнятий пакет (тип, результат, значення, відправник); круглий світлодіод повторює світло |
| Send | розіслати в групи надсилання червоний / зелений / синій / білий / пульс / строб / вимк. або стан лампи |
| Lamp | змінити світло локально (іде в мережу за увімкненої синхронізації «Send») |
| Net | Wi-Fi, IP, почуті вузли; *Forget Wi-Fi* перезавантажує в Improv |
| Settings | параметри синхронізації, групи (2 × 8 кнопок), перевертання екрана на 180°, deep sleep через 30 с / 1 / 3 / 5 хв або ніколи |

Кнопки: ліва — перевести фокус (червона рамка), права — натиснути; коли
екран перевернуто, вони міняються місцями й залишаються «лівою» і «правою»
для вас. Зелений = параметр увімкнено / групу вибрано. Будь-яка кнопка
будить плату з deep sleep.

## Обмеження

- JSON і HTTP-запити через UDP не обробляються.
- Прийняті ефекти вмикаються лише через таблицю `effects:`; RGB-світло
  отримує колір першого світлодіода з даних realtime.
- HTTP API показує ефекти світла й одну палітру.
- На ESP32 компонент використовує 3 UDP-сокети; `/ws` тримає до 4 клієнтів.
