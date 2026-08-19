"""Composites emotion overlays onto content/characters/sveta/assets/calm.png
to produce the other emotion sprites (happy.png, sad.png, ...).

Placeholder art strategy pending real per-emotion illustrations: draws
simple procedural markers (blush, tears, sweat drop, sparkles, Z's, ?, !,
anger mark) onto a copy of the real calm.png artwork rather than inventing
a whole new pose. Re-run after calm.png changes, or edit the anchor
coordinates below if the framing changes.

Requires: pip install pillow
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ASSETS_DIR = Path(__file__).resolve().parent.parent / "content" / "characters" / "sveta" / "assets"
SRC = ASSETS_DIR / "calm.png"
FONT_PATH = Path(r"C:\Windows\Fonts\arialbd.ttf")

base = Image.open(SRC).convert("RGBA")
W, H = base.size  # 1254 x 1254 for the current artwork

# Anchors calibrated against the actual artwork: helmet spans roughly
# x=170..1090, y=0..650; eyes around y=527 (left x=552, right x=777);
# margins for side icons are x<170 (left) and x>1090 (right), y~100-300.
LEFT_MARGIN_X = 100
RIGHT_MARGIN_X = 1150
ICON_Y = 200
LEFT_CHEEK = (500, 590)
RIGHT_CHEEK = (850, 590)


def new_overlay():
    return Image.new("RGBA", (W, H), (0, 0, 0, 0))


def font(size):
    return ImageFont.truetype(str(FONT_PATH), size)


def draw_star(draw, cx, cy, r, fill):
    points = []
    for i in range(10):
        angle = math.pi / 2 + i * math.pi / 5
        radius = r if i % 2 == 0 else r * 0.42
        points.append((cx + radius * math.cos(angle), cy - radius * math.sin(angle)))
    draw.polygon(points, fill=fill)


def draw_teardrop(draw, cx, cy, r, fill):
    # Round bottom + pointed top, classic anime tear/sweat-drop shape.
    draw.ellipse([cx - r, cy - r * 0.2, cx + r, cy + r * 1.8], fill=fill)
    draw.polygon([(cx, cy - r * 1.6), (cx - r * 0.9, cy + r * 0.2), (cx + r * 0.9, cy + r * 0.2)], fill=fill)


def save(name, overlay):
    composited = Image.alpha_composite(base, overlay)
    path = ASSETS_DIR / f"{name}.png"
    composited.save(path)
    print("saved", path)


def main():
    # Happy: soft blush + one small sparkle.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    for cx, cy in (LEFT_CHEEK, RIGHT_CHEEK):
        d.ellipse([cx - 55, cy - 40, cx + 55, cy + 40], fill=(255, 120, 140, 90))
    draw_star(d, RIGHT_MARGIN_X, ICON_Y, 55, (255, 210, 60, 230))
    save("happy", o)

    # Excited: sparkles on both sides.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    draw_star(d, RIGHT_MARGIN_X, ICON_Y, 65, (255, 210, 60, 235))
    draw_star(d, RIGHT_MARGIN_X - 90, ICON_Y + 130, 32, (255, 210, 60, 200))
    draw_star(d, LEFT_MARGIN_X, ICON_Y, 65, (255, 210, 60, 235))
    draw_star(d, LEFT_MARGIN_X + 90, ICON_Y + 130, 32, (255, 210, 60, 200))
    save("excited", o)

    # Curious: tilted question mark.
    o = new_overlay()
    f = font(150)
    qtxt = Image.new("RGBA", (200, 220), (0, 0, 0, 0))
    qd = ImageDraw.Draw(qtxt)
    qd.text((30, 0), "?", font=f, fill=(70, 130, 220, 255))
    qtxt = qtxt.rotate(-12, resample=Image.BICUBIC, expand=True)
    o.alpha_composite(qtxt, (RIGHT_MARGIN_X - 60, ICON_Y - 60))
    save("curious", o)

    # Sad: single teardrop under the eye.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    draw_teardrop(d, 500, 560, 22, (90, 170, 235, 235))
    save("sad", o)

    # Annoyed: red anger cross at the temple.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    cx, cy, s = RIGHT_MARGIN_X, ICON_Y, 70
    d.line([(cx - s, cy - s * 0.3), (cx + s * 0.3, cy + s)], fill=(220, 30, 30, 235), width=22)
    d.line([(cx - s * 0.3, cy + s), (cx + s, cy - s * 0.3)], fill=(220, 30, 30, 235), width=22)
    save("annoyed", o)

    # Embarrassed: strong blush.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    for cx, cy in (LEFT_CHEEK, RIGHT_CHEEK):
        d.ellipse([cx - 60, cy - 45, cx + 60, cy + 45], fill=(255, 90, 120, 165))
    save("embarrassed", o)

    # Sleepy: cascading Z's.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    d.text((RIGHT_MARGIN_X - 40, ICON_Y + 60), "Z", font=font(60), fill=(120, 130, 200, 230))
    d.text((RIGHT_MARGIN_X + 15, ICON_Y + 10), "Z", font=font(85), fill=(120, 130, 200, 235))
    d.text((RIGHT_MARGIN_X + 90, ICON_Y - 60), "z", font=font(110), fill=(120, 130, 200, 240))
    save("sleepy", o)

    # Concerned: blue sweat drop at the temple.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    draw_teardrop(d, RIGHT_MARGIN_X, ICON_Y, 30, (110, 180, 235, 230))
    save("concerned", o)

    # Surprised: big exclamation mark.
    o = new_overlay()
    d = ImageDraw.Draw(o)
    d.text((RIGHT_MARGIN_X - 35, ICON_Y - 90), "!", font=font(190), fill=(230, 40, 40, 255))
    save("surprised", o)

    print("done")


if __name__ == "__main__":
    main()
