# interactive-led-64

An ESP32 project driving a 64x32 HUB75 RGB LED matrix panel over I2S DMA, plus a small Python
utility for converting images into C bitmap headers for display on the panel.

## Hardware

- **MCU:** ESP32 (esp32dev)
- **Display:** 64x32 HUB75 RGB LED matrix panel
- **Driver:** [ESP32 HUB75 LED MATRIX PANEL DMA Display](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA)

### Hardware

- Waveshare 64x32 RGB LED Matrix Panel, 3mm pitch, 2048 individual RGB LEDs (192mm x 96mm)
- ESP32 NodeMCU development board, USB-C, 2.4GHz WiFi/Bluetooth, CH340 chip

### HUB75 to ESP32 wiring

Pin numbers match the vendor's HUB75 INPUT connector diagram (16 at top-left to 1 at
bottom-right).

| HUB75 pin | Signal | ESP32 GPIO | D-label | Board pin # |
| --------- | ------ | ---------- | ------- | ----------- |
| 16        | R1     | GPIO25     | D25     | 10          |
| 15        | G1     | GPIO26     | D26     | 11          |
| 14        | B1     | GPIO27     | D27     | 12          |
| 13        | GND    | ESP32 GND  | GND     | 1 or 38     |
| 12        | R2     | GPIO14     | D14     | 13          |
| 11        | G2     | GPIO13     | D13     | 16          |
| 10        | B2     | GPIO32     | D32     | 8           |
| 9         | E      | -          | -       | -           |
| 8         | A      | GPIO33     | D33     | 9           |
| 7         | B      | GPIO4      | D4      | 26          |
| 6         | C      | GPIO16     | D16     | 27          |
| 5         | D      | GPIO17     | D17     | 28          |
| 4         | CLK    | GPIO18     | D18     | 30          |
| 3         | LAT    | GPIO23     | D23     | 37          |
| 2         | OE     | GPIO19     | D19     | 31          |
| 1         | GND    | ESP32 GND  | GND     | 1 or 38     |

**Note:** Pin 9 (E) is an extra row-address line used on 1/32-scan panels. Leave it
unconnected on standard 1/16-scan or lower panels, or wire it to a free GPIO if your panel
requires it. The 'D' labels are board-specific silkscreen aliases; use the plain GPIO number
in code. 'Board pin #' refers to the physical header pin position on the ESP32 dev board.

The panel is wired directly to the ESP32 (no separate row driver board).

## Firmware

Built with [PlatformIO](https://platformio.org/) using the Arduino framework. On boot, the
firmware initializes the matrix, connects to Wi-Fi via
[WifiConnector](include/WifiConnector.h), then rotates full-screen through its scenes every
10 seconds ([src/main.cpp](src/main.cpp)):

- [ClockDisplay](include/ClockDisplay.h) — the local time (NTP-synced in the timezone
  [LocationService](include/LocationService.h) applies at boot) as a large 24h
  `HH:MM` in rounded seven-segment digits with a pulsing gold colon, a thin bar that fills
  over each minute, and the date ("Mon 26 Oct") in a pill — with a pixel-art Pikachu beside
  the time who hops in, bobs, blinks and sparks.
- [ImageSlider](include/ImageSlider.h) — cycles through the generated `slides[]` array (see
  [include/slides.h](include/slides.h)), a new photo every 3.3 s: each opens through a
  circular iris with a gold ring on its edge (the first one out of the starry sky), then a
  soft shine sweeps across it under a gentle vignette.
- [WeatherDisplay](include/WeatherDisplay.h) — current weather for the configured location
  — an animated condition (sun by day, the moon in its real phase at night, drifting clouds,
  rain, snow, fog, lightning) beside temperature and humidity pills — fetched from the free
  [Open-Meteo API](https://open-meteo.com) (no API key needed). Refetches every 10 minutes.
- [SunMoonDisplay](include/SunMoonDisplay.h) — today's sunrise/sunset, then an eased camera
  pan to the daylight page: a 24h dial with today's daylight swept in gold and a hand at the
  current time, the day length (`9h53`) and its change since yesterday — a cyan `↓ -4m` while
  the days shorten, a gold `↑ +3m` once they lengthen again after the December solstice. In
  a Kurzgesagt-style look: navy gradient sky with twinkling parallax stars and a glowing sun
  with halo rings and turning rays. Times are computed on the board from the configured
  coordinates and the NTP clock by [lib/SkyEvents/](lib/SkyEvents/) — no API.
- [BerlinDisplay](include/BerlinDisplay.h) — a little Berlin skyline: the Fernsehturm (silver
  sphere, red-and-white antenna, blinking aviation light, glinting "Pope's revenge" cross),
  the Berliner Dom and the Rotes Rathaus over the rooftops, with an S-Bahn rolling across a
  brick viaduct. The sky follows the sun's real altitude — blue by day, coral at golden
  hour, starry with the moon in its real phase at night, when the windows light up.
- [BonarBridgeDisplay](include/BonarBridgeDisplay.h) — Bonar Bridge in the Scottish Highlands:
  the steel arch over the Kyle of Sutherland (it grows out from both banks on entry),
  heather hills and pines in parallax, white cottages and a waving Saltire, a car crossing
  and a salmon leaping from the water. Same day/golden-hour/night sky as Berlin, but
  computed for Bonar Bridge itself, so it keeps the Highlands' own hours of daylight.
- [RatEyesDisplay](include/RatEyesDisplay.h) — a photo of a wet rat staring at you, until its
  eyes start beating like a heart — "ba-dum", swelling into big googly eyes with pink love-heart
  pupils and shrinking straight back to small, a few beats then a pause. Twice per visit. The
  photo is `resources/scenes/wet-rat.png`, converted by `image_matrix_creator.py` (pictures in
  `resources/scenes/` get a header but don't become slides); unlike the private slides it's
  committed, and if its header is missing the scene is skipped.
- [ShabbatDisplay](include/ShabbatDisplay.h) — only on Shabbat, from candle lighting (18 min
  before Friday's sunset) to nightfall (42 min after Saturday's, see
  [lib/Shabbat/](lib/Shabbat/)): two candles whose flames catch one after the other and
  flicker, warming the evening sky, a braided challah on a white tablecloth, and
  "Shabbat Shalom" typed in letter by letter in gold, then "Motek" in pink with a beating
  heart. It comes right after the clock;
  the rest of the week the rotation skips it.

**Night mode** — from 23:00 to 07:00 the panel dims (brightness 64 → 16) and the rotation
shrinks to [SleepDisplay](include/SleepDisplay.h): a rat curled up asleep on a cushion,
breathing slowly, with "z"s drifting up and the time in dim blue on a black panel, rendering
only ~7 frames a second. On Shabbat nights Shabbat Shalom alternates with it. Weather fetching
pauses overnight and resumes at 07:00. The hours are `NIGHT_START_HOUR`/`NIGHT_END_HOUR` in
[src/main.cpp](src/main.cpp).

Every scene shares one look and one frame loop — [SceneStyle](include/SceneStyle.h): the
sky, stars, sun, moon, caption pills and motion curves, plus the `style::Scene` base class
(`begin()` on entry, `update()` each loop, one shared framebuffer). Adding a scene is a
subclass and one entry in `main.cpp`'s `scenes[]` list. Weather is fetched on a background FreeRTOS task on core 0, so its HTTPS request
(seconds, longer when the API is down) never stalls the animations rendering on core 1.

Location and timezone are one short config file — see
[Location & timezone](#location--timezone) below.

### Wi-Fi setup

[WifiConnector](include/WifiConnector.h) uses [WiFiManager](https://github.com/tzapu/WiFiManager)
rather than hardcoded credentials. On boot it tries whatever network the ESP32 last connected
to (shown with an animated signal icon). If that fails — first boot, new network, changed
password — it opens its own access point, **`LED-Setup`**, instead of failing silently (the
panel shows "WiFi Setup / Join AP: LED-Setup"). Connect to that AP from a phone or computer;
a captive-portal page should pop up automatically (or browse to `192.168.4.1`) letting you
pick your real network and enter its password. WiFiManager saves it to flash, and the device
won't need reconfiguring again until credentials change. The portal stays open until the
device is online: the setup screen drifts slowly and pulses, so no LED stays lit in one spot,
and every 2 minutes (while nobody is connected to the portal) it retries the saved network, so
a router that was only down for a while is picked up again without a restart — see the
serial-logging note under [Build and flash](#build-and-flash) if you need to debug this.

### Location & timezone

Everything that depends on _where the device is_ — the weather scene, the sun and moon
rise/set times, and the clock's timezone — reads from one file,
`include/config/location_config.h`. No geolocation service, no network lookup:
[LocationService](include/LocationService.h) just hands these values to the scenes that need
them.

That file is **git-ignored** (it pins a physical location). Copy the committed template and
edit it:

```bash
cp include/config/location_config.h.example include/config/location_config.h
```

```c
#define LOCATION_LATITUDE  "52.52"                        // decimal degrees, as strings
#define LOCATION_LONGITUDE "13.405"
#define LOCATION_NAME      "Berlin"                       // logged at boot
#define LOCATION_POSIX_TZ  "CET-1CEST,M3.5.0,M10.5.0/3"   // POSIX TZ rule, DST included
```

If you skip the copy, the build falls back to
[the template](include/config/location_config.h.example) (`src/LocationService.cpp` uses
`__has_include`), so it still compiles and runs — on central-Berlin defaults.

The defaults target Berlin. To run it elsewhere, set your
coordinates and your zone's [POSIX TZ string](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html)
— e.g. `GMT0BST,M3.5.0/1,M10.5.0` (London), `EST5EDT,M3.2.0,M11.1.0` (New York). Because the
rule string encodes the DST transition dates, the C library switches the clock automatically;
there is nothing to refresh and no reboot needed across a DST change.

There are no Wi-Fi credentials to configure anywhere in the repo either — see Wi-Fi setup
above.

### Build and flash

```bash
pio run
pio run --target upload
pio device monitor
```

Dependencies (declared in [platformio.ini](platformio.ini)) are fetched automatically by
PlatformIO:

- `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display`
- `adafruit/Adafruit GFX Library`
- `bblanchon/ArduinoJson`
- `tzapu/WiFiManager`
- `WiFi`, `HTTPClient`, `WiFiClientSecure` (bundled with the ESP32 Arduino core;
  `WebServer` is pulled in transitively by WiFiManager's captive portal)

Firmware logs to serial at 115200 baud (`pio device monitor`), including `WifiConnector`'s
connect/portal status, `LocationService`'s configured location/timezone, `WeatherDisplay`'s
HTTP status and parsed data, and `SunMoonDisplay`'s computed times — useful if the Wi-Fi portal doesn't
behave as expected, or a scene shows "no data".

### Tests

Unity test suites, in two layers:

**Pure-logic suites** run in _both_ environments — on the host with nothing plugged in
(`pio test -e native`) _and_ on the board (`pio test -e esp32dev`). One source file each; the
runner is split on the `ARDUINO` macro (`main()` on the host, `setup()`/`loop()` on-device).
The logic they cover lives in dependency-free modules under [lib/](lib/):

- [test/test_weather_codes/](test/test_weather_codes/) — [lib/WeatherCodes/](lib/WeatherCodes/):
  WMO-code → weather-icon mapping.
- [test/test_clock_text/](test/test_clock_text/) — [lib/ClockText/](lib/ClockText/): the
  zero-padded 24h time and the short date the clock scene draws.
- [test/test_sky_events/](test/test_sky_events/) — [lib/SkyEvents/](lib/SkyEvents/): sun and
  moon rise/set times, moon phase and sun altitude, checked against the US Naval
  Observatory's values for Berlin on three dates across the year.

**Live integration suites** run on-device only (`pio test -e esp32dev`). They reconnect using
whatever network the board last associated with via WiFiManager (run the main firmware and
complete the `LED-Setup` portal first if it never has), then hit the real APIs and assert the
fetch paths actually return usable data — not just that they compile:

- [test/test_weather_display_live/](test/test_weather_display_live/) —
  `WeatherDisplay::fetchWeather()` against the live Open-Meteo API for the configured location.

```bash
pio test -e native       # pure-logic suites, no board required
pio test -e esp32dev     # everything, board connected over USB
```

Run a single suite with `-f <name>`, e.g. `pio test -e esp32dev -f test_weather_display_live`.

## Image-to-bitmap tool

[image_matrix_creator.py](image_matrix_creator.py) batch-converts every image in
[resources/images/](resources/images/) (`.jpg`, `.jpeg`, `.png`, `.bmp`, `.gif`) into a
`PROGMEM` `uint16_t` RGB565 array sized to the panel (64x32), centered on a black canvas.
Each image gets its own header in [include/](include/), named `<filename>_bitmap.h` with a
matching `<filename>Bitmap` array. It also (re)writes [include/slides.h](include/slides.h),
which `#include`s every current bitmap header and exposes them as a `slides[]` array, with no
manual edits needed when images are added or removed. Stale headers for images no longer in
`resources/images/` are deleted automatically.

### Setup

```bash
python3 -m venv rat_daddy_env
source rat_daddy_env/bin/activate
pip install -r requirements.txt
```

### Usage

Drop image files into [resources/images/](resources/images/), then run:

```bash
python3 image_matrix_creator.py
```

Reflash the firmware afterwards to pick up the changes.

### Previewing before flashing

Open [matrix_simulator.html](matrix_simulator.html) in a browser (no server or install
needed) and drop in either source images or the generated `include/*_bitmap.h` headers. It
shows each slide as it will look on the panel by running the pixels through the firmware's
colour path: RGB565, the HUB75 library's `color565to888`, and its CIE 1931 brightness curve.
Headers give the exact flashed pixels. Source images are converted in the browser the same
way `image_matrix_creator.py` does, with slightly different resampling.

## Simulator

[simulate.py](simulate.py) renders every scene on the computer, with no board needed. It
compiles the firmware's real scene code (`src/*.cpp`, unchanged) against a stand-in Arduino
layer and an in-memory 64x32 panel in [sim/](sim/), runs it, and renders the frames as an LED
panel (round LEDs, faint unlit LEDs, soft glow) using the same colour path as the hardware:

```bash
python3 simulate.py                       # every scene
python3 simulate.py --scenes slides       # just the slides
python3 simulate.py --scenes sunmoon,berlin --seconds 10 --fps 60
python3 simulate.py --scenes berlin,bonar --at "2026-06-21 22:30"   # a set date/time
```

Output goes to `captures/sim-<timestamp>/` (git-ignored, since it includes the slides):
`overview.png` with every scene side by side, a folder of PNG stills per scene (one per
second of an animation), and a `<scene>.webp` clip of each (plus `.mp4` if `ffmpeg` is
installed). Time is virtual, so animations render at an exact frame rate. The clock shows the
current time (or the `--at` local time, to see the day/night scenes at any hour), and
weather is fetched live. Needs `clang++` (Xcode command line tools) and the PlatformIO libraries
(`pio pkg install -e esp32dev`, fetched automatically if missing).

## Project layout

```
src/                    Firmware source (main.cpp, WifiConnector.cpp, LocationService.cpp,
                        ClockDisplay.cpp, ImageSlider.cpp, WeatherDisplay.cpp,
                        SunMoonDisplay.cpp, BerlinDisplay.cpp, BonarBridgeDisplay.cpp,
                        ShabbatDisplay.cpp, SleepDisplay.cpp, SceneStyle.cpp)
include/                Project header files, incl. generated bitmap headers,
                        WifiConnector.h, LocationService.h, ClockDisplay.h, ImageSlider.h,
                        WeatherDisplay.h, SunMoonDisplay.h, BerlinDisplay.h,
                        BonarBridgeDisplay.h, ShabbatDisplay.h, SleepDisplay.h,
                        SceneStyle.h
include/config/         location_config.h.example - coordinates, name, POSIX TZ; copy to
                        location_config.h (git-ignored) to set your own
lib/                    Dependency-free, host-testable logic modules (WeatherCodes,
                        ClockText, SkyEvents, Shabbat) - no Arduino/Wi-Fi/matrix includes
test/                   PlatformIO unit tests - pure-logic suites run on both native and
                        esp32dev; *_live suites are on-device only
resources/              Vendor library archive
resources/images/       Source images for image_matrix_creator.py (git-ignored)
sim/                    Host simulator: led_sim driver (simulator.cpp) and the stand-in
                        Arduino/ESP32 headers it compiles the scenes against (arduino/)
```

`src/*.cpp` holds the hardware/network glue for each scene; the decision logic it calls sits
in `lib/` so it can be unit-tested off-device.

## Credits

Inspiered by [ESP32 RGB Matrix Display](https://www.schematik.io/projects/esp32-rgb-matrix-display-wgjb)
on [Schematik](https://www.schematik.io).
