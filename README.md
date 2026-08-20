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
firmware initializes the matrix and alternates full-screen between two scenes every 10
seconds ([src/main.cpp](src/main.cpp)):

- [ClockDisplay](include/ClockDisplay.h) — connects to Wi-Fi, syncs time over NTP, and shows
  a 24h `HH:MM:SS` clock.
- [ImageSlider](include/ImageSlider.h) — cycles through the generated `slides[]` array (see
  [include/slides.h](include/slides.h)) one image at a time.

### Wi-Fi setup

`ClockDisplay` needs Wi-Fi credentials to sync time. Copy the example file and fill in your
network:

```bash
cp include/wifi_credentials.h.example include/wifi_credentials.h
```

Edit `include/wifi_credentials.h` with your `WIFI_SSID` and `WIFI_PASSWORD`. This file is
gitignored and never committed. The clock's timezone is hardcoded to Europe/Berlin
(`CET-1CEST,M3.5.0,M10.5.0/3` in [src/ClockDisplay.cpp](src/ClockDisplay.cpp)); change that
POSIX TZ string if you're elsewhere.

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
- `WiFi` (bundled with the ESP32 Arduino core)

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

## Project layout

```
src/                    Firmware source (main.cpp, ClockDisplay.cpp, ImageSlider.cpp)
include/                Project header files, incl. generated bitmap headers,
                        ClockDisplay.h, ImageSlider.h, and wifi_credentials.h (gitignored)
lib/                    Private/project-specific libraries
test/                   PlatformIO unit tests
resources/              Vendor library archive
resources/images/       Source images for image_matrix_creator.py
```
