"""Builds the MSIX tile and logo assets from the Android launcher artwork.

The Store wants a fixed set of named PNGs plus scale variants. Each one is
resampled from the 512 px source in a single step rather than from an already
shrunk copy, so the small tiles stay sharp.

Square and wide tiles are not the same shape: the wide one is letterboxed on
a transparent ground rather than stretched, because the mark is square.

Run from the repository root:

    python scripts/generate_msix_assets.py
"""

import os

from PIL import Image

SOURCE = os.path.join("android", "play_store_512.png")
OUT_DIR = os.path.join("packaging", "msix-assets")

SQUARE = {
    "StoreLogo": 50,
    "Square44x44Logo": 44,
    "Square71x71Logo": 71,
    "Square150x150Logo": 150,
    "Square310x310Logo": 310,
}

WIDE = {
    "Wide310x150Logo": (310, 150),
    "SplashScreen": (620, 300),
}

SCALES = (100, 125, 150, 200, 400)


def scaled(size, percent):
    return max(1, round(size * percent / 100))


def write_square(source, name, size):
    for percent in SCALES:
        edge = scaled(size, percent)
        tile = source.resize((edge, edge), Image.LANCZOS)
        tile.save(os.path.join(OUT_DIR, "%s.scale-%d.png" % (name, percent)))

    source.resize((size, size), Image.LANCZOS).save(
        os.path.join(OUT_DIR, "%s.png" % name))


def write_wide(source, name, size):
    width, height = size
    for percent in SCALES:
        canvas_w = scaled(width, percent)
        canvas_h = scaled(height, percent)
        mark = source.resize((canvas_h, canvas_h), Image.LANCZOS)
        canvas = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
        canvas.paste(mark, ((canvas_w - canvas_h) // 2, 0), mark)
        canvas.save(os.path.join(OUT_DIR, "%s.scale-%d.png" % (name, percent)))

    mark = source.resize((height, height), Image.LANCZOS)
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    canvas.paste(mark, ((width - height) // 2, 0), mark)
    canvas.save(os.path.join(OUT_DIR, "%s.png" % name))


def main():
    if not os.path.exists(SOURCE):
        raise SystemExit("%s is missing; run from the repository root" % SOURCE)

    os.makedirs(OUT_DIR, exist_ok=True)
    source = Image.open(SOURCE).convert("RGBA")

    for name, size in SQUARE.items():
        write_square(source, name, size)
    for name, size in WIDE.items():
        write_wide(source, name, size)

    written = sorted(os.listdir(OUT_DIR))
    print("%d files in %s" % (len(written), OUT_DIR))
    for name in written:
        path = os.path.join(OUT_DIR, name)
        print("  %-42s %6d bytes" % (name, os.path.getsize(path)))


if __name__ == "__main__":
    main()
