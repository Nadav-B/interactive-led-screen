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
firmware initializes the matrix, sets brightness, and draws a static startup screen
([src/main.cpp](src/main.cpp)).

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

## Image-to-bitmap tool

[image_matrix_creator.py](image_matrix_creator.py) converts a source image into a
`PROGMEM` `uint16_t` RGB565 array sized to the panel (64x32), centered on a black canvas,
and writes it to a C header (e.g. [rat_bitmap.h](rat_bitmap.h)) that can be included from
firmware.

### Setup

```bash
python3 -m venv rat_daddy_env
source rat_daddy_env/bin/activate
pip install -r requirements.txt
```

### Usage

Edit `INPUT_IMAGE`, `OUTPUT_HEADER`, and `ARRAY_NAME` at the top of
[image_matrix_creator.py](image_matrix_creator.py) as needed, then run:

```bash
python3 image_matrix_creator.py
```

## Project layout

```
src/            Firmware source (main.cpp)
include/        Project header files
lib/            Private/project-specific libraries
test/           PlatformIO unit tests
resources/      Reference images and vendor library archive
rat_bitmap.h    Generated bitmap header (output of image_matrix_creator.py)
```
