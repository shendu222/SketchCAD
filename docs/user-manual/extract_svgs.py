#!/usr/bin/env python3
# Extract inline <svg> blocks from the manual HTML into standalone .svg files,
# and render each to a 2x PNG via Edge headless for DOCX embedding.
import os, re, sys, subprocess, html

SRC = r"D:\01-CAD\02-avticationCAD\sketch-cad-cpp\docs\user-manual\SketchCAD用户手册.html"
IMG = r"D:\01-CAD\02-avticationCAD\sketch-cad-cpp\docs\user-manual\img"
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

os.makedirs(IMG, exist_ok=True)
with open(SRC, encoding="utf-8") as f:
    txt = f.read()

# crude but sufficient: match top-level <svg ...>...</svg>
blocks = re.findall(r"<svg\b.*?</svg>", txt, flags=re.S)
print(f"found {len(blocks)} svg blocks")

infos = []
for i, b in enumerate(blocks):
    m = re.search(r'viewBox="([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)"', b)
    if not m:
        m = re.search(r'viewBox=[\'"]([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)[\'"]', b)
    vbw, vbh = (float(m.group(3)), float(m.group(4))) if m else (760, 460)
    # normalize: set explicit width/height, ensure xmlns
    b2 = re.sub(r'<svg\b', '<svg xmlns="http://www.w3.org/2000/svg"', b, count=1)
    b2 = re.sub(r'width="[^"]*"', f'width="{int(vbw)}"', b2, count=1)
    b2 = re.sub(r'height="[^"]*"', f'height="{int(vbh)}"', b2, count=1)
    if 'xmlns=' not in b2.split('>', 1)[0]:
        b2 = b2.replace('<svg', '<svg xmlns="http://www.w3.org/2000/svg"', 1)
    svgpath = os.path.join(IMG, f"_svg_{i}.svg")
    with open(svgpath, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n' + b2)
    infos.append((i, int(vbw), int(vbh), svgpath))
    print(f"  svg {i}: {int(vbw)}x{int(vbh)} -> {svgpath}")

# render each to PNG
def render(svgpath, pngpath, w, h):
    tmphtml = svgpath + ".tmp.html"
    with open(tmphtml, "w", encoding="utf-8") as f:
        f.write('<!doctype html><html><head><meta charset="utf-8"></head>'
                '<body style="margin:0;padding:0">' + open(svgpath, encoding="utf-8").read() + '</body></html>')
    cmd = [EDGE, "--headless", "--disable-gpu", "--no-sandbox",
           "--hide-scrollbars",
           f"--window-size={w},{h}",
           "--force-device-scale-factor=2",
           f"--screenshot={pngpath}",
           "file:///" + tmphtml.replace("\\", "/")]
    try:
        subprocess.run(cmd, check=True, timeout=60, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError as e:
        # retry with old headless mode
        cmd[1] = "--headless=old"
        subprocess.run(cmd, check=True, timeout=60, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        os.remove(tmphtml)
    except OSError:
        pass

for i, w, h, svgpath in infos:
    pngpath = os.path.join(IMG, f"_svg_{i}.png")
    render(svgpath, pngpath, w, h)
    if os.path.exists(pngpath):
        print(f"  rendered svg {i} -> _svg_{i}.png ({os.path.getsize(pngpath)} bytes)")
    else:
        print(f"  FAILED svg {i}")

print("DONE")
