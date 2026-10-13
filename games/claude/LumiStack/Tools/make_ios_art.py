#!/usr/bin/env python3
"""Renders the iOS app icon and launch image procedurally (no external art).

Uses the same stage-1 palette as the game core so the icon matches the first screen.
Outputs:
  Build/IOS/Resources/Assets.xcassets/AppIcon.appiconset/Icon-1024.png  (+ Contents.json)
  Build/IOS/Resources/Graphics/Icon1024.png                              (legacy UE layout)
  Build/IOS/Resources/Graphics/LaunchScreenIOS.png
Requires Pillow.
"""
import colorsys
import json
import pathlib

from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent
SKY_TOP = (0.99, 0.84, 0.71)       # stage DAWN
SKY_BOTTOM = (0.56, 0.64, 0.88)


def rgb(c):
    return tuple(int(max(0, min(1, v)) * 255 + 0.5) for v in c)


def gradient(w, h, top, bottom):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        t = (y / (h - 1)) ** 1.15
        c = rgb(tuple(a + (b - a) * t for a, b in zip(top, bottom)))
        for x in range(w):
            px[x, y] = c
    return img


def shade(c, k):
    return tuple(int(max(0, min(255, v * k))) for v in c)


def draw_box(d, cx, cy, sx, sy, h, color, unit):
    """Isometric box. (cx, cy) is the screen position of the top-face centre."""
    ax = (unit * 0.866, unit * 0.5)    # world +X on screen
    ay = (-unit * 0.866, unit * 0.5)   # world +Y on screen
    def p(x, y, z):
        return (cx + ax[0] * x + ay[0] * y, cy + ax[1] * x + ay[1] * y - z * unit)
    hx, hy = sx / 2, sy / 2
    top = [p(-hx, -hy, 0), p(hx, -hy, 0), p(hx, hy, 0), p(-hx, hy, 0)]
    front_x = [p(hx, -hy, 0), p(hx, hy, 0), p(hx, hy, -h), p(hx, -hy, -h)]
    front_y = [p(-hx, hy, 0), p(hx, hy, 0), p(hx, hy, -h), p(-hx, hy, -h)]
    d.polygon(front_x, fill=shade(color, 0.72))
    d.polygon(front_y, fill=shade(color, 0.86))
    d.polygon(top, fill=color)


def tower(size, top_y, unit, floors=7, base_h=6.0):
    img = gradient(size, size, SKY_TOP, SKY_BOTTOM)
    glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = size * 0.36
    gd.ellipse((size / 2 - r, top_y - r * 0.9, size / 2 + r, top_y + r * 0.9), fill=(255, 255, 255, 70))
    glow = glow.filter(ImageFilter.GaussianBlur(size * 0.06))
    img.paste(glow, (0, 0), glow)
    d = ImageDraw.Draw(img)
    h = 0.5
    cx = size / 2
    # pedestal
    base_top = top_y + floors * h * unit
    draw_box(d, cx, base_top, 3.2, 3.2, base_h, (238, 235, 243), unit)
    # stacked floors, slightly offset so the stack reads as hand-built
    offsets = [(0, 0), (0.0, -0.08), (0.12, -0.08), (0.12, 0.0), (0.05, 0.0), (0.05, 0.1), (0.0, 0.1)]
    for i in range(floors):
        f = i + 1
        hue = (335 + 3.5 * f * 2.2) % 360
        col = rgb(colorsys.hsv_to_rgb(hue / 360, 0.52, 0.96))
        ox, oy = offsets[i % len(offsets)]
        size_x = 3.2 - 0.12 * min(i, 3)
        size_y = 3.2 - 0.08 * min(i, 4)
        draw_box(d, cx + (ox - oy) * unit * 0.866, base_top - f * h * unit + (ox + oy) * unit * 0.5, size_x, size_y, h, col, unit)
    return img


def main():
    icon_dir = ROOT / "Build/IOS/Resources/Assets.xcassets/AppIcon.appiconset"
    gfx_dir = ROOT / "Build/IOS/Resources/Graphics"
    icon_dir.mkdir(parents=True, exist_ok=True)
    gfx_dir.mkdir(parents=True, exist_ok=True)

    icon = tower(1024, top_y=360, unit=104, floors=8)
    icon.save(icon_dir / "Icon-1024.png")
    icon.save(gfx_dir / "Icon1024.png")
    (icon_dir / "Contents.json").write_text(json.dumps({
        "images": [{"filename": "Icon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024"}],
        "info": {"author": "xcode", "version": 1},
    }, indent=2) + "\n")
    (icon_dir.parent / "Contents.json").write_text(json.dumps({"info": {"author": "xcode", "version": 1}}, indent=2) + "\n")

    launch = gradient(1170, 2532, SKY_TOP, SKY_BOTTOM)
    launch.save(gfx_dir / "LaunchScreenIOS.png")
    print("wrote", icon_dir / "Icon-1024.png", "and launch image")


if __name__ == "__main__":
    main()
