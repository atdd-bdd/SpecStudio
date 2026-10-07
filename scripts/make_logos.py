"""Draw the AlignThree logo at the sizes an MSIX package needs.

The mark is the website favicon (website/assets/favicon.svg): a rounded square
in AlignThree violet, three white discs at the corners of a triangle, and the
triangle itself drawn faintly between them. There is no icon file anywhere in
the repository to start from, so it is drawn here rather than converted -- the
shape is three circles and a rounded rectangle, which is less code than a
dependency on an SVG renderer.

Writes into resources/logos/, which the MSIX layout copies as Assets/. Run it
again after changing the mark; the output is deterministic.

    python scripts\\make_logos.py

Every size Windows asks a desktop MSIX for, and what uses it:

    Square44x44Logo     the taskbar and the Start list, at five scales
    Square150x150Logo   the medium Start tile, at five scales
    StoreLogo           the Store listing and the installer dialog
"""
import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("Pillow is needed to draw the logos:  pip install pillow")

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "resources", "logos")

VIOLET = (76, 70, 214, 255)
WHITE = (255, 255, 255, 255)
FAINT = (255, 255, 255, 115)        # the triangle, opacity .45

# The mark in the SVG's own 24x24 coordinates.
RADIUS = 5.0 / 24.0                 # rounded-corner radius
DISCS = [(12.0 / 24, 5.4 / 24), (5.0 / 24, 18.0 / 24), (19.0 / 24, 18.0 / 24)]
DISC_R = 2.3 / 24.0
STROKE = 1.4 / 24.0

# Supersample, then shrink: Pillow draws no antialiasing of its own, and at
# 44 pixels a hard-edged circle looks like a cog.
SS = 8


def draw(size, plated=True):
    """One logo, `size` pixels square. plated=False leaves it transparent."""
    n = size * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    if plated:
        d.rounded_rectangle([0, 0, n - 1, n - 1], radius=RADIUS * n, fill=VIOLET)

    pts = [(x * n, y * n) for x, y in DISCS]

    # The triangle between the discs, faint, closed.
    width = max(1, int(round(STROKE * n)))
    d.line([pts[0], pts[1], pts[2], pts[0]], fill=FAINT, width=width, joint="curve")

    r = DISC_R * n
    for x, y in pts:
        d.ellipse([x - r, y - r, x + r, y + r], fill=WHITE)

    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    written = []

    # name, base size: Windows wants each at five scales.
    for name, base in (("Square44x44Logo", 44), ("Square150x150Logo", 150)):
        for scale in (100, 125, 150, 200, 400):
            size = int(round(base * scale / 100))
            path = os.path.join(OUT, f"{name}.scale-{scale}.png")
            draw(size).save(path)
            written.append((os.path.basename(path), size))

    # The 44x44 also has unplated target sizes, used where Windows draws the
    # icon on its own background: the taskbar's small icons, Alt+Tab, the
    # title bar. Without them Windows scales the plated tile and the violet
    # square shows up where it should not.
    for size in (16, 24, 32, 48, 256):
        path = os.path.join(OUT, f"Square44x44Logo.targetsize-{size}.png")
        draw(size).save(path)
        written.append((os.path.basename(path), size))
        path = os.path.join(OUT, f"Square44x44Logo.targetsize-{size}_altform-unplated.png")
        draw(size, plated=False).save(path)
        written.append((os.path.basename(path), size))

    for scale in (100, 125, 150, 200, 400):
        size = int(round(50 * scale / 100))
        path = os.path.join(OUT, f"StoreLogo.scale-{scale}.png")
        draw(size).save(path)
        written.append((os.path.basename(path), size))

    print(f"{len(written)} logos in {OUT}")
    for name, size in written:
        print(f"  {name:56} {size}x{size}")


if __name__ == "__main__":
    main()
