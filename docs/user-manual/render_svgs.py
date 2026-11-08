#!/usr/bin/env python3
# Render the 4 inline SVG diagrams to crisp PNGs (2x) for DOCX embedding.
import os, glob
from svglib.svglib import svg2rlg, register_font
from reportlab.graphics import renderPM
from PIL import Image

IMG = r"D:\01-CAD\02-avticationCAD\sketch-cad-cpp\docs\user-manual\img"

# Register CJK fonts in svglib's own font map (reportlab registration is NOT used by svglib)
register_font("Microsoft YaHei", font_path=r"C:\Windows\Fonts\msyh.ttc")
register_font("Microsoft YaHei", weight="bold", font_path=r"C:\Windows\Fonts\msyhbd.ttc")
register_font("sans-serif", font_path=r"C:\Windows\Fonts\msyh.ttc")
register_font("sans-serif", weight="bold", font_path=r"C:\Windows\Fonts\msyhbd.ttc")
register_font("SimHei", font_path=r"C:\Windows\Fonts\simhei.ttf")
print("fonts registered")

# map svg index -> desired pixel width multiplier (relative to viewBox width)
for i in range(4):
    svgp = os.path.join(IMG, f"_svg_{i}.svg")
    pngp = os.path.join(IMG, f"_svg_{i}.png")
    if not os.path.exists(svgp):
        print(f"skip {svgp} (not found)")
        continue
    d = svg2rlg(svgp)
    if d is None:
        print(f"FAILED to parse {svgp}")
        continue
    # render at high dpi
    renderPM.drawToFile(d, pngp, fmt="PNG", dpi=300, bg=0xFFFFFF)
    # ensure white background + upscale to ~2x viewBox width for crispness
    im = Image.open(pngp).convert("RGBA")
    # composite onto white
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    im = Image.alpha_composite(bg, im).convert("RGB")
    # upscale if smaller than 2x viewBox width
    vbw = int(d.width * 1.3333)  # svg2rlg uses 72dpi mapping; recover approx width
    target = max(1400, int(d.width * 2))
    if im.width < target:
        ratio = target / im.width
        im = im.resize((target, int(im.height * ratio)), Image.LANCZOS)
    im.save(pngp, "PNG")
    print(f"rendered {svgp} -> {pngp}  ({im.width}x{im.height})")

# cleanup edge test artifacts
for f in glob.glob(os.path.join(IMG, "test*.png")) + glob.glob(os.path.join(IMG, "_svg_*.wrap.html")):
    try:
        os.remove(f)
        print("cleaned", f)
    except OSError:
        pass
print("DONE")
