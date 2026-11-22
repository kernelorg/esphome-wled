[English](WLED_EFFECTS.md) | **Русский** | [Українська](WLED_EFFECTS.uk.md) | [Polski](WLED_EFFECTS.pl.md)

# Эффекты WLED и их ID

Справочник для сопоставления эффектов света ESPHome с эффектами WLED. В
пакетах синхронизации WLED эффект передаётся номером (`fx`); здесь
перечислены все номера **WLED 16.0.1** (220 эффектов), взятые с устройства
WLED из `/json/effects` и `/json/fxdata`. Проверить свой узел:
`http://<ip-wled>/json/effects` — позиция в этом списке и есть ID.

WLED сохраняет номера неизменными и добавляет новые эффекты в конец; номера
`RSVD` зарезервированы (не используются).

## Столбцы

| Столбец | Значение |
|---|---|
| 1 LED | ✓ — эффект работает и на одном светодиоде / одном ШИМ-канале (флаг WLED `0`): подходит для монохромной или одноцветной лампы |
| 2D | ✓ — для светодиодных матриц |
| Звук | `volume` / `frequency` — реагирует на звук (нужен usermod AudioReactive) |

## Эффекты для одного светодиода

Для лампы с одним ШИМ-каналом имеют смысл только эти:

| ID | Название | Звук |
|---|---|---|
| 0 | Solid |  |
| 1 | Blink |  |
| 2 | Breathe |  |
| 5 | Random Colors |  |
| 8 | Colorloop |  |
| 12 | Fade |  |
| 23 | Strobe |  |
| 24 | Strobe Rainbow |  |
| 25 | Strobe Mega |  |
| 26 | Blink Rainbow |  |
| 45 | Fire Flicker |  |
| 88 | Candle |  |
| 100 | Heartbeat |  |
| 116 | TV Simulator |  |
| 130 | Juggles | volume |
| 133 | Plasmoid | volume |
| 137 | Freqwave | frequency |
| 138 | Freqmatrix | frequency |
| 140 | Waterfall | frequency |
| 143 | Noisefire | volume |
| 145 | Noisemove | frequency |
| 159 | DJ Light | frequency |
| 185 | Rocktaves | frequency |

## Подходящие аналоги эффектов ESPHome

Отправная точка — выбирайте то, что на вашей лампе выглядит ближе всего. В
сопоставлении первый ID отправляется, все перечисленные распознаются при
приёме:

| Эффект ESPHome | Отправлять как | Распознавать также |
|---|---|---|
| `pulse` | 2 Breathe | 12 Fade, 100 Heartbeat |
| `strobe` | 23 Strobe | 1 Blink, 24 Strobe Rainbow, 25 Strobe Mega, 26 Blink Rainbow |
| `flicker` | 88 Candle | 45 Fire Flicker |
| `random` | 5 Random Colors | — |
| `addressable_rainbow` | 9 Rainbow | 8 Colorloop |
| `addressable_color_wipe` | 3 Wipe | 4 Wipe Random |
| `addressable_scan` | 10 Scan | 11 Scan Dual |
| `addressable_twinkle` | 17 Twinkle | 80 Twinklefox |
| `addressable_fireworks` | 42 Fireworks | — |
| `addressable_flicker` | 45 Fire Flicker | 88 Candle |

Например:

```yaml
wled_emu:
  effects:
    - effect: "Pulse"            # имя эффекта света ESPHome
      wled:
        - 2                      # Breathe (отправляется)
        - 12                     # Fade
        - 100                    # Heartbeat
    - effect: "Strobe"
      wled:
        - 23                     # Strobe (отправляется)
        - 24                     # Strobe Rainbow
        - 25                     # Strobe Mega
    - effect: "Candle"
      wled:
        - 88                     # Candle
```

## Все эффекты

| ID | Название | 1 LED | 2D | Звук |
|---|---|---|---|---|
| 0 | Solid | ✓ |  |  |
| 1 | Blink | ✓ |  |  |
| 2 | Breathe | ✓ |  |  |
| 3 | Wipe |  |  |  |
| 4 | Wipe Random |  |  |  |
| 5 | Random Colors | ✓ |  |  |
| 6 | Sweep |  |  |  |
| 7 | Dynamic |  |  |  |
| 8 | Colorloop | ✓ |  |  |
| 9 | Rainbow |  |  |  |
| 10 | Scan |  |  |  |
| 11 | Scan Dual |  |  |  |
| 12 | Fade | ✓ |  |  |
| 13 | Theater |  |  |  |
| 14 | Theater Rainbow |  |  |  |
| 15 | Running |  |  |  |
| 16 | Saw |  |  |  |
| 17 | Twinkle |  |  |  |
| 18 | Dissolve |  |  |  |
| 19 | Dissolve Rnd |  |  |  |
| 20 | Sparkle |  |  |  |
| 21 | Sparkle Dark |  |  |  |
| 22 | Sparkle+ |  |  |  |
| 23 | Strobe | ✓ |  |  |
| 24 | Strobe Rainbow | ✓ |  |  |
| 25 | Strobe Mega | ✓ |  |  |
| 26 | Blink Rainbow | ✓ |  |  |
| 27 | Android |  |  |  |
| 28 | Chase |  |  |  |
| 29 | Chase Random |  |  |  |
| 30 | Chase Rainbow |  |  |  |
| 31 | Chase Flash |  |  |  |
| 32 | Chase Flash Rnd |  |  |  |
| 33 | Rainbow Runner |  |  |  |
| 34 | Colorful |  |  |  |
| 35 | Traffic Light |  |  |  |
| 36 | Sweep Random |  |  |  |
| 37 | Chase 2 |  |  |  |
| 38 | Aurora |  |  |  |
| 39 | Stream |  |  |  |
| 40 | Scanner |  |  |  |
| 41 | Lighthouse |  |  |  |
| 42 | Fireworks |  | ✓ |  |
| 43 | Rain |  | ✓ |  |
| 44 | Tetrix |  |  |  |
| 45 | Fire Flicker | ✓ |  |  |
| 46 | Gradient |  |  |  |
| 47 | Loading |  |  |  |
| 48 | Rolling Balls |  |  |  |
| 49 | Fairy |  |  |  |
| 50 | Two Dots |  |  |  |
| 51 | Fairytwinkle |  |  |  |
| 52 | Running Dual |  |  |  |
| 53 | RSVD |  |  |  |
| 54 | Chase 3 |  |  |  |
| 55 | Tri Wipe |  |  |  |
| 56 | Tri Fade |  |  |  |
| 57 | Lightning |  |  |  |
| 58 | ICU |  |  |  |
| 59 | Multi Comet |  |  |  |
| 60 | Scanner Dual |  |  |  |
| 61 | Stream 2 |  |  |  |
| 62 | Oscillate |  |  |  |
| 63 | Pride 2015 |  |  |  |
| 64 | Juggle |  |  |  |
| 65 | Palette |  | ✓ |  |
| 66 | Fire 2012 |  |  |  |
| 67 | Colorwaves |  |  |  |
| 68 | Bpm |  |  |  |
| 69 | Fill Noise |  |  |  |
| 70 | Noise 1 |  |  |  |
| 71 | Noise 2 |  |  |  |
| 72 | Noise 3 |  |  |  |
| 73 | Noise 4 |  |  |  |
| 74 | Colortwinkles |  |  |  |
| 75 | Lake |  |  |  |
| 76 | Meteor |  |  |  |
| 77 | Copy Segment |  | ✓ |  |
| 78 | Railway |  |  |  |
| 79 | Ripple |  | ✓ |  |
| 80 | Twinklefox |  |  |  |
| 81 | Twinklecat |  |  |  |
| 82 | Halloween Eyes |  | ✓ |  |
| 83 | Solid Pattern |  |  |  |
| 84 | Solid Pattern Tri |  |  |  |
| 85 | Spots |  |  |  |
| 86 | Spots Fade |  |  |  |
| 87 | Glitter |  |  |  |
| 88 | Candle | ✓ |  |  |
| 89 | Fireworks Starburst |  |  |  |
| 90 | Fireworks 1D |  | ✓ |  |
| 91 | Bouncing Balls |  |  |  |
| 92 | Sinelon |  |  |  |
| 93 | Sinelon Dual |  |  |  |
| 94 | Sinelon Rainbow |  |  |  |
| 95 | Popcorn |  |  |  |
| 96 | Drip |  |  |  |
| 97 | Plasma |  |  |  |
| 98 | Percent |  |  |  |
| 99 | Ripple Rainbow |  | ✓ |  |
| 100 | Heartbeat | ✓ |  |  |
| 101 | Pacifica |  |  |  |
| 102 | Candle Multi |  |  |  |
| 103 | Solid Glitter |  |  |  |
| 104 | Sunrise |  |  |  |
| 105 | Phased |  |  |  |
| 106 | Twinkleup |  |  |  |
| 107 | Noise Pal |  |  |  |
| 108 | Sine |  |  |  |
| 109 | Phased Noise |  |  |  |
| 110 | Flow |  |  |  |
| 111 | Chunchun |  |  |  |
| 112 | Dancing Shadows |  |  |  |
| 113 | Washing Machine |  |  |  |
| 114 | Rotozoomer |  | ✓ |  |
| 115 | Blends |  |  |  |
| 116 | TV Simulator | ✓ |  |  |
| 117 | Dynamic Smooth |  |  |  |
| 118 | Spaceships |  | ✓ |  |
| 119 | Crazy Bees |  | ✓ |  |
| 120 | Ghost Rider |  | ✓ |  |
| 121 | Blobs |  | ✓ |  |
| 122 | Scrolling Text |  | ✓ |  |
| 123 | Drift Rose |  | ✓ |  |
| 124 | Distortion Waves |  | ✓ |  |
| 125 | Soap |  | ✓ |  |
| 126 | Octopus |  | ✓ |  |
| 127 | Waving Cell |  | ✓ |  |
| 128 | Pixels |  |  | volume |
| 129 | Pixelwave |  |  | volume |
| 130 | Juggles | ✓ |  | volume |
| 131 | Matripix |  |  | volume |
| 132 | Gravimeter |  |  | volume |
| 133 | Plasmoid | ✓ |  | volume |
| 134 | Puddles |  |  | volume |
| 135 | Midnoise |  |  | volume |
| 136 | Noisemeter |  |  | volume |
| 137 | Freqwave | ✓ |  | frequency |
| 138 | Freqmatrix | ✓ |  | frequency |
| 139 | GEQ |  | ✓ | frequency |
| 140 | Waterfall | ✓ |  | frequency |
| 141 | Freqpixels |  |  | frequency |
| 142 | RSVD |  |  |  |
| 143 | Noisefire | ✓ |  | volume |
| 144 | Puddlepeak |  |  | volume |
| 145 | Noisemove | ✓ |  | frequency |
| 146 | Noise2D |  | ✓ |  |
| 147 | Perlin Move |  |  |  |
| 148 | Ripple Peak |  |  | volume |
| 149 | Firenoise |  | ✓ |  |
| 150 | Squared Swirl |  | ✓ |  |
| 151 | PacMan |  |  |  |
| 152 | DNA |  | ✓ |  |
| 153 | Matrix |  | ✓ |  |
| 154 | Metaballs |  | ✓ |  |
| 155 | Freqmap |  |  | frequency |
| 156 | Gravcenter |  |  | volume |
| 157 | Gravcentric |  |  | volume |
| 158 | Gravfreq |  |  | frequency |
| 159 | DJ Light | ✓ |  | frequency |
| 160 | Funky Plank |  | ✓ | frequency |
| 161 | Shimmer |  |  |  |
| 162 | Pulser |  | ✓ |  |
| 163 | Blurz |  |  | frequency |
| 164 | Drift |  | ✓ |  |
| 165 | Waverly |  | ✓ | volume |
| 166 | Sun Radiation |  | ✓ |  |
| 167 | Colored Bursts |  | ✓ |  |
| 168 | Julia |  | ✓ |  |
| 169 | RSVD |  |  |  |
| 170 | RSVD |  |  |  |
| 171 | RSVD |  |  |  |
| 172 | Game Of Life |  | ✓ |  |
| 173 | Tartan |  | ✓ |  |
| 174 | Polar Lights |  | ✓ |  |
| 175 | Swirl |  | ✓ | volume |
| 176 | Lissajous |  | ✓ |  |
| 177 | Frizzles |  | ✓ |  |
| 178 | Plasma Ball |  | ✓ |  |
| 179 | Flow Stripe |  |  |  |
| 180 | Hiphotic |  | ✓ |  |
| 181 | Sindots |  | ✓ |  |
| 182 | DNA Spiral |  | ✓ |  |
| 183 | Black Hole |  | ✓ |  |
| 184 | Wavesins |  |  |  |
| 185 | Rocktaves | ✓ |  | frequency |
| 186 | Akemi |  | ✓ | frequency |
| 187 | RSVD |  |  |  |
| 188 | RSVD |  |  |  |
| 189 | RSVD |  |  |  |
| 190 | RSVD |  |  |  |
| 191 | RSVD |  |  |  |
| 192 | RSVD |  |  |  |
| 193 | RSVD |  |  |  |
| 194 | RSVD |  |  |  |
| 195 | RSVD |  |  |  |
| 196 | RSVD |  |  |  |
| 197 | RSVD |  |  |  |
| 198 | RSVD |  |  |  |
| 199 | RSVD |  |  |  |
| 200 | RSVD |  |  |  |
| 201 | RSVD |  |  |  |
| 202 | PS DripDrop |  |  |  |
| 203 | PS Pinball |  |  |  |
| 204 | PS Dancing Shadows |  |  |  |
| 205 | PS Fireworks 1D |  |  |  |
| 206 | PS Sparkler |  |  |  |
| 207 | PS Hourglass |  |  |  |
| 208 | PS Spray 1D |  |  |  |
| 209 | PS 1D Balance |  |  |  |
| 210 | PS Chase |  |  |  |
| 211 | PS Starburst |  |  |  |
| 212 | PS GEQ 1D |  |  | frequency |
| 213 | PS Fire 1D |  |  |  |
| 214 | PS Sonic Stream |  |  | frequency |
| 215 | PS Sonic Boom |  |  | frequency |
| 216 | PS Springy |  |  | frequency |
| 217 | RSVD |  |  |  |
| 218 | Color Clouds |  |  |  |
| 219 | Slow Transition |  |  |  |
