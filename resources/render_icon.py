#!/usr/bin/env python3
"""Rasterize the app icon (resources/icon.svg) into the PNG sizes used by the
Qt resource bundle. Kept in-tree because ImageMagick's built-in SVG renderer
drops the gradients; this draws the same geometry with Pillow instead.

Usage: python3 resources/render_icon.py
"""
import os

from PIL import Image, ImageDraw

SS = 4  # supersampling factor
SIZES = [16, 24, 32, 48, 64, 128, 256, 512]
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icons")

# Warm amber ramp, matching the app's #e2944a accent.
TOP = (255, 193, 114)
MID = (226, 148, 74)
BOTTOM = (196, 112, 42)


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def base_gradient(size):
    """Vertical amber ramp with a slight rightward tilt (TOP -> MID -> BOTTOM)."""
    grad = Image.new("RGB", (size, size))
    px = grad.load()
    for y in range(size):
        for x in range(size):
            # Direction roughly (0,0) -> (0.35, 1.0), as in icon.svg.
            t = min(1.0, max(0.0, (0.35 * x + 1.0 * y) / (1.35 * size)))
            px[x, y] = lerp(TOP, MID, t / 0.5) if t < 0.5 else lerp(MID, BOTTOM, (t - 0.5) / 0.5)
    return grad


def sheen(size):
    """Soft white highlight falling off from the top-left corner."""
    layer = Image.new("L", (size, size))
    px = layer.load()
    for y in range(size):
        for x in range(size):
            t = min(1.0, (0.2 * x + 1.0 * y) / (0.55 * 1.2 * size))
            px[x, y] = round(0.28 * 255 * (1.0 - t))
    return layer


def render(size):
    s = size * SS
    k = s / 512.0  # scale from the 512-unit SVG coordinate space

    canvas = base_gradient(s).convert("RGBA")
    white = Image.new("RGBA", (s, s), (255, 255, 255, 255))
    canvas = Image.composite(white, canvas, sheen(s))

    # Squircle mask.
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [24 * k, 24 * k, 488 * k, 488 * k], radius=112 * k, fill=255
    )
    icon = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    icon.paste(canvas, (0, 0), mask)

    d = ImageDraw.Draw(icon)
    stroke = round(36 * k)

    # Prompt chevron.
    d.line(
        [(150 * k, 182 * k), (222 * k, 256 * k), (150 * k, 330 * k)],
        fill=(255, 255, 255, 255),
        width=stroke,
        joint="curve",
    )
    for cx, cy in ((150 * k, 182 * k), (222 * k, 256 * k), (150 * k, 330 * k)):
        r = stroke / 2
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255, 255))

    # Baseline bar under the text lines.
    d.rounded_rectangle(
        [286 * k - stroke / 2, 330 * k - stroke / 2, 374 * k + stroke / 2, 330 * k + stroke / 2],
        radius=stroke / 2,
        fill=(255, 255, 255, 255),
    )

    # Two snippet lines, slightly recessed.
    lines = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ld = ImageDraw.Draw(lines)
    ld.rounded_rectangle([268 * k, 166 * k, 374 * k, 198 * k], radius=16 * k, fill=(255, 255, 255, 140))
    ld.rounded_rectangle([268 * k, 230 * k, 340 * k, 262 * k], radius=16 * k, fill=(255, 255, 255, 140))
    icon = Image.alpha_composite(icon, lines)

    return icon.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    images = []
    for size in SIZES:
        img = render(size)
        img.save(os.path.join(OUT_DIR, "app-%d.png" % size))
        images.append(img)
        print("wrote app-%d.png" % size)

    # Windows .ico bundling the standard shell sizes.
    ico_sizes = [s for s in (16, 24, 32, 48, 64, 128, 256) if s in SIZES]
    render(256).save(
        os.path.join(OUT_DIR, "app.ico"),
        format="ICO",
        sizes=[(s, s) for s in ico_sizes],
    )
    print("wrote app.ico")


if __name__ == "__main__":
    main()
