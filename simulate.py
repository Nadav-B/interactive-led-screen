"""Render the LED panel's scenes on the computer - no board needed.

Compiles the firmware's real scene code (src/*.cpp) against a simulated
64x32 panel (sim/), runs it, and renders the frames as LED-panel images:

    python3 simulate.py                         # every scene
    python3 simulate.py --scenes slides         # just the slides
    python3 simulate.py --scenes sunmoon,berlin --seconds 10 --fps 60

Writes to captures/sim-<timestamp>/ (git-ignored - it contains your slides):
    overview.png        one image per scene, side by side
    <scene>/NN.png      stills: one per second of an animation
    <scene>.webp        animated clip of each scene
    <scene>.mp4         the same clip as video, if ffmpeg is installed

Weather is fetched live (needs internet), the clock shows
the current time, and slides come from include/slides.h - run
image_matrix_creator.py first to update them.
"""
import argparse
import shutil
import struct
import subprocess
import sys
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent
LIBDEPS = ROOT / ".pio" / "libdeps" / "esp32dev"
BUILD_DIR = ROOT / ".pio" / "sim"
BINARY = BUILD_DIR / "led_sim"

WIDTH, HEIGHT = 64, 32
FRAME_BYTES = WIDTH * HEIGHT * 2
ALL_SCENES = ["clock", "slides", "sunmoon", "weather", "berlin"]

# Scene sources compiled into the simulator. main.cpp (board setup and the
# scene rotation) and WifiConnector.cpp (Wi-Fi portal) are board-only.
SCENE_SOURCES = ["BerlinDisplay", "ClockDisplay", "ImageSlider",
                 "LocationService", "SceneStyle", "SunMoonDisplay", "WeatherDisplay"]

# --- Build -----------------------------------------------------------------

def build():
    gfx = LIBDEPS / "Adafruit GFX Library"
    json = LIBDEPS / "ArduinoJson" / "src"
    if not gfx.exists() or not json.exists():
        print("Fetching libraries (pio pkg install -e esp32dev)...")
        pio = shutil.which("pio") or str(Path.home() / ".platformio/penv/bin/pio")
        subprocess.run([pio, "pkg", "install", "-e", "esp32dev"], cwd=ROOT, check=True)

    sources = [ROOT / "src" / f"{name}.cpp" for name in SCENE_SOURCES]
    sources += sorted((ROOT / "lib").glob("*/*.cpp"))
    sources += sorted((ROOT / "sim").glob("*.cpp"))
    sources.append(gfx / "Adafruit_GFX.cpp")
    includes = [ROOT / "sim" / "arduino", ROOT / "include", gfx, json]
    includes += [p for p in sorted((ROOT / "lib").iterdir()) if p.is_dir()]

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = ["clang++", "-std=c++17", "-O2", "-w",
           "-DARDUINO=100",
           # ArduinoJson: take Arduino Strings, but no Stream/Print/PROGMEM.
           "-DARDUINOJSON_ENABLE_ARDUINO_STRING=1", "-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0",
           "-DARDUINOJSON_ENABLE_ARDUINO_PRINT=0", "-DARDUINOJSON_ENABLE_PROGMEM=0"]
    cmd += [f"-I{p}" for p in includes]
    cmd += [str(s) for s in sources] + ["-o", str(BINARY)]
    print("Building simulator...")
    subprocess.run(cmd, check=True)

# --- Colour: same path as the panel ----------------------------------------
# RGB565 -> the HUB75 library's color565to888 -> its CIE 1931 brightness
# table -> sRGB, so a screen emits (relatively) what the LEDs emit.

def _cie_to_screen(v):
    lightness = v / 255 * 100
    y = lightness / 902.3 if lightness <= 8 else ((lightness + 16) / 116) ** 3
    duty = round(y * 255) / 255
    s = 12.92 * duty if duty <= 0.0031308 else 1.055 * duty ** (1 / 2.4) - 0.055
    return round(s * 255)

LED_TO_SCREEN = [_cie_to_screen(v) for v in range(256)]

def frame_to_image(raw):
    """64x32 RGB image of one raw RGB565 frame, as seen on the panel."""
    out = bytearray(WIDTH * HEIGHT * 3)
    for i, (c,) in enumerate(struct.iter_unpack("<H", raw)):
        r, g, b = (c >> 8) & 0xF8, (c >> 3) & 0xFC, (c << 3) & 0xF8
        out[i * 3] = LED_TO_SCREEN[r | r >> 5]
        out[i * 3 + 1] = LED_TO_SCREEN[g | g >> 6]
        out[i * 3 + 2] = LED_TO_SCREEN[b | b >> 5]
    return Image.frombytes("RGB", (WIDTH, HEIGHT), bytes(out))

# --- LED-panel look --------------------------------------------------------

class PanelRenderer:
    """Scales a 64x32 frame up into round LEDs on a dark panel, with the
    unlit LEDs faintly visible and a soft glow, like a photo of the panel."""

    def __init__(self, scale, padding_leds=2):
        self.scale = scale
        self.pad = padding_leds * scale
        size = (WIDTH * scale, HEIGHT * scale)

        # One antialiased round LED, tiled across the panel.
        sprite = Image.new("L", (scale * 4, scale * 4), 0)
        r = scale * 4 * 0.40
        c = scale * 4 / 2
        ImageDraw.Draw(sprite).ellipse([c - r, c - r, c + r, c + r], fill=255)
        sprite = sprite.resize((scale, scale), Image.LANCZOS)
        mask = Image.new("L", size)
        for y in range(HEIGHT):
            for x in range(WIDTH):
                mask.paste(sprite, (x * scale, y * scale))
        self.mask = Image.merge("RGB", (mask, mask, mask))
        self.unlit = ImageChops.multiply(self.mask, Image.new("RGB", size, (22, 22, 24)))
        self.glow_tint = Image.new("RGB", size, (90, 90, 90))

    def render(self, frame):
        size = (WIDTH * self.scale, HEIGHT * self.scale)
        leds = ImageChops.multiply(frame.resize(size, Image.NEAREST), self.mask)
        leds = ImageChops.lighter(leds, self.unlit)
        glow = frame.resize((WIDTH * 4, HEIGHT * 4), Image.NEAREST)
        glow = glow.filter(ImageFilter.GaussianBlur(3)).resize(size, Image.BILINEAR)
        panel = ImageChops.add(leds, ImageChops.multiply(glow, self.glow_tint))

        canvas = Image.new("RGB", (size[0] + 2 * self.pad, size[1] + 2 * self.pad), (8, 8, 9))
        canvas.paste(panel, (self.pad, self.pad))
        return canvas

# --- Output ----------------------------------------------------------------

def load_frames(path):
    data = path.read_bytes()
    step = 4 + FRAME_BYTES
    return [(struct.unpack_from("<I", data, i)[0], data[i + 4:i + step])
            for i in range(0, len(data) - step + 1, step)]

def frame_at(frames, t_ms):
    """Index of the frame on screen at t_ms."""
    idx = 0
    while idx + 1 < len(frames) and frames[idx + 1][0] <= t_ms:
        idx += 1
    return idx

def write_scene(name, frames, renderer, out_dir, fps):
    """Writes the scene's stills and clip; returns one representative
    image for the overview."""
    scene_dir = out_dir / name
    scene_dir.mkdir(exist_ok=True)

    rendered = {}
    def image(idx):
        if idx not in rendered:
            rendered[idx] = renderer.render(frame_to_image(frames[idx][1]))
        return rendered[idx]

    duration = frames[-1][0]
    for second in range(int(duration // 1000) + 1):
        image(frame_at(frames, second * 1000)).save(scene_dir / f"{second:02d}.png")

    clip = [image(i) for i in range(len(frames))]
    frame_ms = round(1000 / fps)
    clip[0].save(out_dir / f"{name}.webp", save_all=True, append_images=clip[1:],
                 duration=frame_ms, loop=0, quality=85)
    if shutil.which("ffmpeg"):
        w, h = clip[0].size
        proc = subprocess.Popen(
            ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
             "-s", f"{w}x{h}", "-r", str(fps), "-i", "-", "-c:v", "libx264",
             "-pix_fmt", "yuv420p", "-crf", "16", str(out_dir / f"{name}.mp4")],
            stdin=subprocess.PIPE)
        for img in clip:
            proc.stdin.write(img.tobytes())
        proc.stdin.close()
        proc.wait()
    # A moment into the animation reads better than its very first frame.
    return image(frame_at(frames, min(2000, duration)))

def write_overview(images, out_dir):
    cols = 3
    w, h = images[0].size
    gap = w // 40
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w + (cols + 1) * gap, rows * h + (rows + 1) * gap), (24, 24, 26))
    for n, img in enumerate(images):
        sheet.paste(img, (gap + (n % cols) * (w + gap), gap + (n // cols) * (h + gap)))
    sheet.save(out_dir / "overview.png")

def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--scenes", default=",".join(ALL_SCENES),
                        help=f"comma-separated subset of: {', '.join(ALL_SCENES)}")
    parser.add_argument("--seconds", type=int, default=8, help="length of animated clips")
    parser.add_argument("--fps", type=int, default=30, help="frame rate of animated clips")
    parser.add_argument("--scale", type=int, default=20,
                        help="pixels per LED (20 -> 1360x720 images)")
    parser.add_argument("--seed", type=int, default=0,
                        help="shifts the simulated boot moment")
    args = parser.parse_args()

    scenes = [s.strip() for s in args.scenes.split(",") if s.strip()]
    unknown = [s for s in scenes if s not in ALL_SCENES]
    if unknown:
        sys.exit(f"Unknown scene(s): {', '.join(unknown)}")

    build()
    out_dir = ROOT / "captures" / datetime.now().strftime("sim-%Y-%m-%d_%H-%M-%S")
    out_dir.mkdir(parents=True)
    subprocess.run([str(BINARY), "--out", str(out_dir), "--fps", str(args.fps),
                    "--seconds", str(args.seconds), "--seed", str(args.seed),
                    "--scenes", ",".join(scenes)], check=True)

    renderer = PanelRenderer(args.scale)
    overview = []
    for name in scenes:
        raw = out_dir / f"{name}.bin"
        if not raw.exists():
            continue
        frames = load_frames(raw)
        print(f"Rendering {name} ({len(frames)} frames)...")
        overview.append(write_scene(name, frames, renderer, out_dir, args.fps))
        raw.unlink()
    if overview:
        write_overview(overview, out_dir)
    if not shutil.which("ffmpeg"):
        print("(ffmpeg not found - clips are .webp only; brew install ffmpeg for .mp4)")
    print(f"Done: {out_dir.relative_to(ROOT)}/")

if __name__ == "__main__":
    main()
