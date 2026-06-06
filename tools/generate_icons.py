from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "src" / "assets"
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def rounded_rect(draw: ImageDraw.ImageDraw, xy, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def polygon_scaled(draw: ImageDraw.ImageDraw, points, scale, offset, fill, outline=None, width=1):
    scaled = [(offset[0] + x * scale, offset[1] + y * scale) for x, y in points]
    draw.polygon(scaled, fill=fill)
    if outline:
        draw.line(scaled + [scaled[0]], fill=outline, width=width, joint="curve")


def draw_keyboard_cursor(size: int, active: bool, app_icon: bool) -> Image.Image:
    scale = size / 256
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    bg = (22, 29, 39, 255)
    bg2 = (31, 41, 55, 255)
    cyan = (57, 213, 255, 255)
    cyan_dark = (7, 117, 152, 255)
    green = (80, 235, 157, 255)
    muted = (105, 121, 139, 255)
    white = (236, 246, 255, 255)

    pad = 18 * scale
    rounded_rect(d, (pad, pad, size - pad, size - pad), 42 * scale, bg)
    rounded_rect(d, (pad + 8 * scale, pad + 8 * scale, size - pad - 8 * scale, size - pad - 8 * scale), 34 * scale, bg2)

    if active:
        d.ellipse((31 * scale, 31 * scale, 225 * scale, 225 * scale), outline=green, width=max(2, round(12 * scale)))
    elif app_icon:
        d.arc((31 * scale, 31 * scale, 225 * scale, 225 * scale), 205, 336, fill=cyan, width=max(2, round(10 * scale)))

    key_w = 35 * scale
    key_gap = 8 * scale
    key_radius = max(2, 7 * scale)
    start_x = 53 * scale
    start_y = 61 * scale
    keys = [(1, 0), (0, 1), (1, 1), (2, 1)]
    for col, row in keys:
        x = start_x + col * (key_w + key_gap)
        y = start_y + row * (key_w + key_gap)
        fill = (47, 61, 79, 255) if not active else (43, 71, 75, 255)
        outline = green if active and (col, row) == (1, 1) else muted
        rounded_rect(d, (x, y, x + key_w, y + key_w), key_radius, fill, outline, max(1, round(2 * scale)))

    # Cursor pointer, simplified enough to survive tray sizes.
    cursor = [
        (125, 69),
        (125, 187),
        (151, 163),
        (169, 205),
        (193, 195),
        (174, 154),
        (209, 154),
    ]
    polygon_scaled(d, cursor, scale, (0, 0), white, (8, 17, 26, 255), max(2, round(5 * scale)))

    accent = [(141, 91), (141, 155), (158, 140), (180, 140)]
    polygon_scaled(d, accent, scale, (0, 0), cyan if not active else green)

    if size >= 32:
        d.line((132 * scale, 77 * scale, 132 * scale, 177 * scale), fill=(255, 255, 255, 92), width=max(1, round(2 * scale)))
        d.line((142 * scale, 159 * scale, 162 * scale, 141 * scale), fill=cyan_dark, width=max(1, round(3 * scale)))

    return img


def save_ico(name: str, active: bool, app_icon: bool):
    images = [draw_keyboard_cursor(size, active, app_icon) for size in SIZES]
    images[-1].save(ASSET_DIR / name, sizes=[(s, s) for s in SIZES], append_images=images[:-1])


def main():
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    save_ico("app.ico", active=True, app_icon=True)
    save_ico("tray_off.ico", active=False, app_icon=False)
    save_ico("tray_on.ico", active=True, app_icon=False)


if __name__ == "__main__":
    main()
