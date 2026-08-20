import re
from pathlib import Path

from PIL import Image

# --- Settings ---
RESOURCES_DIR = Path("resources/images")   # source images live here
INCLUDE_DIR = Path("include")       # generated headers are written here
PANEL_WIDTH = 64
PANEL_HEIGHT = 32
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".gif"}

def rgb_to_565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def array_name_for(image_path):
    name = re.sub(r"[^0-9a-zA-Z]+", "_", image_path.stem).strip("_")
    if name and name[0].isdigit():
        name = f"_{name}"
    return f"{name}Bitmap"

def convert_image(image_path, header_path, array_name):
    img = Image.open(image_path).convert("RGB")

    # Resize to fit the panel, preserving aspect ratio, centered on black canvas
    img.thumbnail((PANEL_WIDTH, PANEL_HEIGHT), Image.LANCZOS)
    canvas = Image.new("RGB", (PANEL_WIDTH, PANEL_HEIGHT), (0, 0, 0))
    offset_x = (PANEL_WIDTH - img.width) // 2
    offset_y = (PANEL_HEIGHT - img.height) // 2
    canvas.paste(img, (offset_x, offset_y))

    pixels = list(canvas.get_flattened_data())

    with open(header_path, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(f"const uint16_t {array_name}[{PANEL_WIDTH * PANEL_HEIGHT}] PROGMEM = {{\n")

        for i, (r, g, b) in enumerate(pixels):
            val = rgb_to_565(r, g, b)
            f.write(f"0x{val:04X}, ")
            if (i + 1) % PANEL_WIDTH == 0:
                f.write("\n")

        f.write("};\n")

def write_slides_header(entries):
    """Write include/slides.h, which #includes every generated bitmap header
    and exposes them as a slides[] array, so main.cpp never needs manual edits."""
    slides_path = INCLUDE_DIR / "slides.h"
    with open(slides_path, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n\n")
        for header_name, _ in entries:
            f.write(f'#include "{header_name}"\n')
        f.write("\nconst uint16_t *const slides[] = {\n")
        for _, array_name in entries:
            f.write(f"  {array_name},\n")
        f.write("};\n")
        f.write("constexpr size_t SLIDE_COUNT = sizeof(slides) / sizeof(slides[0]);\n")
    print(f"Wrote {slides_path} ({len(entries)} slide(s))")

def main():
    INCLUDE_DIR.mkdir(exist_ok=True)

    images = sorted(
        p for p in RESOURCES_DIR.iterdir()
        if p.is_file() and p.suffix.lower() in IMAGE_EXTENSIONS
    )

    if not images:
        print(f"No images found in {RESOURCES_DIR}/")
        return

    entries = []
    for image_path in images:
        array_name = array_name_for(image_path)
        header_name = f"{image_path.stem}_bitmap.h"
        header_path = INCLUDE_DIR / header_name
        convert_image(image_path, header_path, array_name)
        print(f"Wrote {PANEL_WIDTH * PANEL_HEIGHT} pixels to {header_path} ({array_name})")
        entries.append((header_name, array_name))

    # Remove generated headers for images no longer in RESOURCES_DIR.
    current_headers = {header_name for header_name, _ in entries}
    for stale in INCLUDE_DIR.glob("*_bitmap.h"):
        if stale.name not in current_headers:
            stale.unlink()
            print(f"Removed stale header {stale}")

    write_slides_header(entries)

if __name__ == "__main__":
    main()
