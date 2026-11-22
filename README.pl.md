[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | **Polski**

# esphome-wled

Zewnętrzny komponent ESPHome `wled_emu`: urządzenie z ESPHome zachowuje się w
sieci lokalnej jak węzeł **WLED**. Podąża za innymi węzłami WLED i samo nimi
steruje, jest widoczne na ich listach węzłów i w aplikacjach WLED, odpowiada
na HTTP / JSON API WLED. „Taśmą LED” może być dowolne światło ESPHome:
monochromatyczny PWM, RGB, …

| Funkcja | Opis |
|---|---|
| Synchronizacja UDP (port 21324) | Odbiór i wysyłanie jasności, koloru i efektu; 8 grup synchronizacji; powtórzenie z flagą followUp po 250 ms; przez 500 ms nic nie jest odbijane z powrotem. Pakiety mają compatibilityVersion 10, więc węzły z „Receive Segment bounds” zachowują swoje segmenty |
| Realtime | WARLS / DRGB / DRGBW / DNRGB / DNRGBW z limitem czasu, TPM2.NET (z odpowiedzią na ping). Po zakończeniu realtime wraca poprzedni stan (wł., jasność, kolor, efekt) |
| Wykrywanie | Rozgłaszanie node-info (port 65506) co 10 s; tabela słyszanych węzłów WLED z limitem 60 s |
| HTTP API (`http_api: true`) | `/json`, `/json/state` (GET + POST), `/json/info`, `/json/si`, `/json/nodes`, `/json/effects`, `/json/palettes`, `/json/cfg`, `/json/sync`, `/win&T=..&A=..`; strony `/sync` (macierz grup) i `/nodes` |
| WebSocket `/ws` (ESP32) | Wysyła `{"state","info"}` po połączeniu, przy każdej zmianie i co 30 s; przyjmuje ten sam JSON co POST `/json/state`. Aplikacja WLED (WLED Native) pokazuje węzeł jako „online” i z sygnałem Wi-Fi tylko przez to połączenie |
| Encje | [przełączniki](#przełączniki) dla ustawień i grup synchronizacji (zapisywane), [czujniki, czujniki tekstowe, liczby](#czujniki-czujniki-tekstowe-liczby) dla węzłów, pakietów, ostatniego pakietu, realtime i portów UDP — bez lambd |
| Platformy | ESP8266 (Arduino) i ESP32 (Arduino / esp-idf) |

Bez `light_id` komponent tylko wysyła (pilot / kontroler): port
synchronizacji nie jest otwierany, `send_notifier` i wykrywanie działają. Z
`light_id` stosuje przychodzącą synchronizację / realtime do światła i
rozgłasza zmiany samego światła (jasność, kolor, efekt; sprawdzane co
250 ms).

## Konfiguracja

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
  light_id: lamp             # światło podążające za WLED; bez niego — tylko wysyłanie
  http_api: true             # wymaga web_server (i light_id)
  sync_send: true
  sync_receive: true
  send_groups: 0x01
  receive_groups: 0x01
  effects:                   # efekt światła <-> numery efektów WLED (WLED_EFFECTS.pl.md)
    - effect: "Pulse"
      wled:
        - 2                    # Breathe: wysyłany dla „Pulse”
        - 12                   # Fade: przy odbiorze też włącza „Pulse”
    - effect: "Strobe"
      wled:
        - 23                   # Strobe
  on_node_seen:
    - logger.log:
        format: "new WLED peer %s (%s)"
        args:
          - 'ip.c_str()'
          - 'name.c_str()'
  on_groups_change:          # np. zmienione na stronie /sync
    - logger.log:
        format: "groups: send 0x%02X, receive 0x%02X"
        args:
          - 'send'
          - 'recv'

# Dla aplikacji WLED: wykrywanie przez mDNS, HTTP API na porcie 80.
mdns:
  services:
    - service: "_wled"
      protocol: "_tcp"
      port: 80
web_server:
  port: 80
```

| Opcja | Domyślnie | |
|---|---|---|
| `light_id` | — | światło sterowane synchronizacją / realtime WLED i rozgłaszane przy zmianie |
| `http_api` | `false` | HTTP / JSON API WLED i `/ws`; wymaga `web_server` i `light_id` |
| `port` | `21324` | port UDP synchronizacji / realtime |
| `info_port` | `65506` | port node-info (wykrywanie); WLED przed v16 używał `port + 1` |
| `sync_enable` | `true` | główny wyłącznik wysyłania i odbioru |
| `sync_send` / `sync_receive` | `false` / `true` | wysyłaj własne zmiany / stosuj odebrane pakiety |
| `receive_brightness` / `receive_effects` | `true` / `true` | które części odebranego pakietu są stosowane |
| `send_on_change` | `true` | automatycznie rozgłaszaj zmiany światła |
| `retransmit` | `true` | powtórz każdy pakiet raz (followUp) po 250 ms |
| `send_groups` / `receive_groups` | `0x01` / `0x01` | maski grup synchronizacji, bit 0 = grupa 1 |
| `effects` | — | lista `effect:` (nazwa efektu światła) + `wled:` (jeden lub kilka numerów efektów WLED), zob. [Efekty](#efekty) |
| `discovery` | `true` | nasłuchuj node-info i prowadź tabelę węzłów |
| `sysinfo_interval` | `10s` | odstęp rozgłaszania node-info |
| `node_timeout` | `60s` | milczący węzeł jest usuwany po tym czasie |
| `max_nodes` | `16` | rozmiar tabeli węzłów |
| `node_type` | auto | typ zgłaszany węzłom (32 = ESP32, 210 = ESP8266) |

## Efekty

W pakietach synchronizacji WLED efekt jest przesyłany jako numer efektu WLED
(Breathe = 2, Strobe = 23, …), a światło ESPHome ma efekty nazwane. Lista
`effects:` określa, którym efektom WLED odpowiada każdy efekt światła:

- **odbiór:** dowolny z wymienionych numerów włącza ten efekt światła;
  numer spoza listy (lub 0, Solid) wyłącza efekt;
- **wysyłanie:** efekt światła idzie swoim **pierwszym** numerem; efekt
  spoza listy idzie jako 0 (Solid);
- kilka efektów światła może dzielić jeden numer — przy odbiorze wygrywa
  pierwszy na liście;
- nazwy są sprawdzane z efektami światła podczas walidacji konfiguracji.

Wszystkie numery WLED i które z nich działają na jednej diodzie (lampa
monochromatyczna): [WLED_EFFECTS.pl.md](WLED_EFFECTS.pl.md). HTTP API i
aplikacja WLED pokazują wszystkie efekty światła (przypisane i nie) i
przełączają je bezpośrednio.

## Akcje

```yaml
- wled_emu.send_notifier:      # do grup wysyłania lub do `groups:`
    "on": true                 # w cudzysłowie: gołe `on` w YAML to wartość logiczna
    brightness: 200
    red: 255
    green: 0
    blue: 0
    effect: "Pulse"            # efekt światła → jego pierwszy numer WLED z `effects:`
    # wled_effect: 2           # … albo numer efektu WLED bezpośrednio (domyślnie 0 = Solid)
    groups: 0x03
- wled_emu.send_state:         # bieżący stan powiązanego światła
```

Wszystkie pola mogą być szablonami (`!lambda`). Przykład — przycisk dzwonka
sprawia, że wszystkie lampy grup 1 i 2 migają:

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

## Przełączniki

Ustawienia pracy i grupy synchronizacji jako przełączniki — dla Home
Assistant lub strony WWW, bez lambd:

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

| Opcja | |
|---|---|
| `type` | `sync`, `sync_send`, `sync_receive`, `receive_brightness`, `receive_effects`, `send_on_change`, `retransmit`, `send_group`, `receive_group` |
| `group` | `1`..`8`, dla `send_group` / `receive_group` |
| `wled_emu_id` | który `wled_emu` przełączać (tylko gdy jest ich kilka) |
| `restore_mode` | domyślnie `RESTORE_DEFAULT_OFF`: po restarcie stosowana jest zapisana wartość; dopóki nic nie zapisano, zostaje wartość z konfiguracji `wled_emu`. `ALWAYS_ON` / `ALWAYS_OFF` wymuszają wartość przy starcie |

Zmiany wprowadzone gdzie indziej — na stronie `/sync`, przez `/json/sync`, w
interfejsie diagnostycznym, z lambd — są widoczne na przełącznikach i
zapisywane razem z nimi.

## Czujniki, czujniki tekstowe, liczby

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

| Platforma | `type` | Wartość |
|---|---|---|
| `sensor` | `peers` | słyszane węzły WLED |
| | `rx_packets` / `tx_packets` | pakiety odebrane na porcie synchronizacji / rozgłoszone (od startu) |
| `binary_sensor` | `realtime` | światłem sterują dane realtime |
| `text_sensor` | `last_packet` | np. `notifier applied: bri 200 rgb 255/0/0 fx 2 grp 01`, `DRGB applied: …`, `notifier other group: …` |
| | `last_sender` | IP nadawcy ostatniego pakietu |
| | `peers` | `nazwa (ip), …` (do 255 znaków) |
| `number` | `port` / `info_port` | port UDP synchronizacji / wykrywania; zapisywany (`restore_value`, domyślnie `true`), gniazdo jest od razu otwierane ponownie |

Wartości są sprawdzane raz na sekundę i publikowane tylko przy zmianie.

## Wyzwalacze

| Wyzwalacz | Zmienne | |
|---|---|---|
| `on_node_seen` | `ip`, `name` (`std::string`) | usłyszano nowy węzeł WLED |
| `on_groups_change` | `send`, `recv` (`uint8_t`) | zmieniły się maski grup (strona `/sync`, `/json/sync`, lambda) |

## API z lambd

| Metoda | |
|---|---|
| `send_notifier(on, bri, r, g, b, wled_effect[, groups])`, `send_state()`, `send_sysinfo()` | wyślij teraz (`wled_effect` — numer efektu WLED) |
| `fxmode_for_effect(name)`, `effect_for_fxmode(id)` | wyszukiwanie w tabeli `effects:` |
| `set_sync_enable/send/receive(bool)`, `set_receive_brightness/effects(bool)`, `set_send_on_change(bool)`, `set_retransmit(bool)` i odpowiednie `get_…()` | przełączniki w trakcie pracy (same nie są zapisywane — do tego służą [przełączniki](#przełączniki)) |
| `set_send_groups(m)`, `set_receive_groups(m)`, `set_group_bit(send, 1..8, on)`, `get_send_groups()`, `get_receive_groups()` | maski grup |
| `set_port(p)`, `set_info_port(p)` | gniazda są otwierane ponownie w następnej pętli |
| `nodes()` → `std::vector<WledNode>{ip, name, type, vid, last_seen}`, `node_count()` | tabela węzłów; `type & 0x80` = światło węzła włączone |
| `rx_count()`, `tx_count()` | pakiety odebrane na porcie synchronizacji / rozgłoszone |
| `last_rx_kind()`, `last_rx_result()`, `last_rx_detail()`, `last_rx_ip()`, `last_rx_age_ms()` | ostatni odebrany pakiet, np. `"notifier"`, `"other group"`, `"bri 200 rgb 255/0/0 fx 2 grp 01"` |
| `in_realtime()` | światłem sterują teraz dane realtime |
| `add_on_settings_change_callback(f)`, `add_on_groups_change_callback(f)` | wywoływane przy zmianie przełącznika pracy / masek grup |

## Aplikacje WLED

- Węzeł jest wykrywany przez mDNS (`_wled._tcp`, zob. konfigurację wyżej);
  telefon i węzeł muszą być w jednej sieci albo router musi przekazywać mDNS.
- **ESP32:** aplikacja WLED łączy się z `/ws` i pokazuje węzeł „online” z
  sygnałem Wi-Fi; z aplikacji działają wł./wył., jasność, kolor i efekt. Po
  otwarciu węzła aplikacja pokazuje własną stronę urządzenia — serwer WWW
  ESPHome.
- **ESP8266:** WebSocket jeszcze nie ma, więc w aplikacji WLED węzeł jest
  „offline”. Synchronizacja z innymi węzłami, HTTP API i listy innych węzłów
  działają.

## Przykłady

| Plik | |
|---|---|
| [`example/wled_mono_lamp_esp8266.yaml`](example/wled_mono_lamp_esp8266.yaml) | monochromatyczna lampa PWM na NodeMCU (D1, aktywny stan niski, 1 kHz): podąża za synchronizacją i realtime WLED, sześć efektów, przełączniki synchronizacji (platforma switch) zachowane po restarcie, przycisk „puls wszystkim lampom”, czujniki stanu, porty UDP |
| [`example/wled_emu_esp32.yaml`](example/wled_emu_esp32.yaml) | pilot na ESP32 bez światła: przyciski wysyłają do grup, czujnik liczy węzły |
| [`example/wled_lvgl_tdisplay.yaml`](example/wled_lvgl_tdisplay.yaml) | firmware diagnostyczny LilyGO T-Display: wirtualna lampa RGB na ekranie + interfejs diagnostyczny, wszystkie typy encji; Wi-Fi przez Improv BLE |

Skopiuj `example/secrets.yaml.example` do `example/secrets.yaml` i zbuduj z
katalogu głównego repozytorium:

```bash
docker run --rm -v "$PWD":/config -w /config ghcr.io/esphome/esphome:2026.10.1 \
  compile example/wled_mono_lamp_esp8266.yaml
```

## Opcjonalny interfejs diagnostyczny (LVGL)

[`ui/lvgl_debug.yaml`](ui/lvgl_debug.yaml) to pakiet dla małego poziomego
wyświetlacza (240×135) z dwoma przyciskami (ESP32, esp-idf); do zwykłej pracy
nie jest potrzebny.

```yaml
packages:
  wled_ui: !include
    file: <path>/esphome-wled/ui/lvgl_debug.yaml
    vars:
      wled_id: wled
      light_id: lamp             # światło powiązane z wled_emu
      display_id: lcd
      rotation: "90"             # obrót poziomy bez odwrócenia
      key_left: btn_left         # binary_sensor: lewy przycisk
      key_right: btn_right       # binary_sensor: prawy przycisk
      backlight_id: backlight
      deep_sleep_id: deep_sleep_ctl
```

| Strona | |
|---|---|
| Status | IP, Wi-Fi, przełączniki synchronizacji, maski grup, węzły, liczniki, ostatni odebrany pakiet (rodzaj, wynik, wartości, nadawca); okrągła dioda powtarza światło |
| Send | rozgłoś do grup wysyłania czerwony / zielony / niebieski / biały / puls / strobo / wył. albo stan lampy |
| Lamp | zmień światło lokalnie (idzie do sieci przy włączonej synchronizacji „Send”) |
| Net | Wi-Fi, IP, słyszane węzły; *Forget Wi-Fi* uruchamia ponownie w trybie Improv |
| Settings | opcje synchronizacji, grupy (2 × 8 przycisków), obrót ekranu o 180°, deep sleep po 30 s / 1 / 3 / 5 min lub nigdy |

Przyciski: lewy — przesuń fokus (czerwona ramka), prawy — naciśnij; po
odwróceniu ekranu zamieniają się rolami, więc pozostają „lewym” i „prawym”
dla Ciebie. Zielony = opcja włączona / grupa wybrana. Dowolny przycisk budzi
płytkę z deep sleep.

## Ograniczenia

- JSON i żądania HTTP przez UDP nie są obsługiwane.
- Odebrane efekty włączają się tylko przez tabelę `effects:`; światło RGB
  dostaje kolor pierwszej diody z danych realtime.
- HTTP API pokazuje efekty światła i jedną paletę.
- Na ESP32 komponent używa 3 gniazd UDP; `/ws` obsługuje do 4 klientów.
