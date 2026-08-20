from PIL import Image

# --- Settings ---
INPUT_IMAGE = "./resources/two_rats.jpg"       # your source image
OUTPUT_HEADER = "include/rat_bitmap.h"
PANEL_WIDTH = 64
PANEL_HEIGHT = 32
ARRAY_NAME = "ratBitmap"

def rgb_to_565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def main():
    img = Image.open(INPUT_IMAGE).convert("RGB")

    # Resize to fit the panel, preserving aspect ratio, centered on black canvas
    img.thumbnail((PANEL_WIDTH, PANEL_HEIGHT), Image.LANCZOS)
    canvas = Image.new("RGB", (PANEL_WIDTH, PANEL_HEIGHT), (0, 0, 0))
    offset_x = (PANEL_WIDTH - img.width) // 2
    offset_y = (PANEL_HEIGHT - img.height) // 2
    canvas.paste(img, (offset_x, offset_y))

    pixels = list(canvas.get_flattened_data())

    # Write the .h file
    with open(OUTPUT_HEADER, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(f"const uint16_t {ARRAY_NAME}[{PANEL_WIDTH * PANEL_HEIGHT}] PROGMEM = {{\n")

        for i, (r, g, b) in enumerate(pixels):
            val = rgb_to_565(r, g, b)
            f.write(f"0x{val:04X}, ")
            if (i + 1) % PANEL_WIDTH == 0:
                f.write("\n")

        f.write("};\n")

    print(f"Done. Wrote {PANEL_WIDTH*PANEL_HEIGHT} pixels to {OUTPUT_HEADER}")

if __name__ == "__main__":
    main()