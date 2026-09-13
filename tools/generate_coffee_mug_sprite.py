"""One-off generator for content/items/coffee_mug.png (Phase 9 Item System).
Run manually after editing; output is committed like any other content
asset, not regenerated at app runtime.

Requires: pip install pillow
"""
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "content" / "items" / "coffee_mug.png"
SIZE = 96  # on-screen size; see items::ItemWindow's default sizing
CANVAS = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(CANVAS)

MUG_BROWN = (109, 68, 36, 255)      # body fill
MUG_BROWN_DARK = (82, 50, 26, 255)  # outline
COFFEE = (63, 38, 20, 255)          # liquid at the rim
STEAM = (255, 255, 255, 130)        # translucent steam squiggle

# Body: rounded rect, leaves top margin for steam and bottom margin so
# nothing touches the canvas edge.
BODY = [22, 30, 70, 78]
draw.rounded_rectangle(BODY, radius=8, fill=MUG_BROWN, outline=MUG_BROWN_DARK, width=3)

# Coffee fill visible at the rim (a thin ellipse just inside the top edge).
draw.ellipse([26, 32, 66, 40], fill=COFFEE)

# Handle: a ring on the right side, drawn as a thick ellipse outline.
draw.ellipse([64, 40, 88, 66], outline=MUG_BROWN_DARK, width=6)

# Steam: two simple squiggles above the mug.
for cx in (36, 52):
    pts = [(cx, 24), (cx + 5, 18), (cx - 5, 12), (cx, 6)]
    draw.line(pts, fill=STEAM, width=3, joint="curve")

CANVAS.save(OUT)
print(f"Wrote {OUT} ({SIZE}x{SIZE})")
