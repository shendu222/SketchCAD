#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 SketchCAD 版本发布海报 + 下载地址二维码。

设计基调沿用项目既有资源 resources/poster_sketchcad_v1.0.jpg：
深蓝工程蓝图底纹 / 细网格 / 青蓝中心线 / 橙色中心十字 / 白色描边标题。
所有文案均取自项目已有事实（README.md、RELEASE_NOTES.md、LICENSE.txt、
installer.iss、dist/ 产物文件名），不引入任何虚构功能、界面截图或测试数据。

输出：
  resources/posters/SketchCAD-v1.1.1-poster.png       发布海报成稿
  resources/posters/SketchCAD-v1.1.1-download-qr.png   下载地址二维码（可单独使用）

依赖：Pillow、qrcode、segno、zxing-cpp（后两者仅用于交叉校验二维码内容）。
运行：python tools/gen_release_poster.py
"""

from __future__ import annotations

import math
import os
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
import qrcode
import segno
import zxingcpp

# ---------------------------------------------------------------------------
# 唯一数据源：与 README.md / RELEASE_NOTES.md / 安装包文件名保持同源
# ---------------------------------------------------------------------------
VERSION = "1.1.1"
TAG = "v1.1.1-setup"
DOWNLOAD_URL = "https://github.com/shendu222/SketchCAD/releases/tag/v1.1.1-setup"
RELEASE_DATE = "2026-11-08"          # RELEASE_NOTES.md
PUBLISHER = "我是慎独"                # LICENSE.txt / installer.iss
EXE_NAME = "SketchCAD-1.1.1-Setup.exe"   # dist/ 实际产物
MSI_NAME = "SketchCAD-1.1.1-Setup.msi"   # dist/ 实际产物

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "resources" / "posters"
POSTER_PATH = OUT_DIR / f"SketchCAD-v{VERSION}-poster.png"
QR_PATH = OUT_DIR / f"SketchCAD-v{VERSION}-download-qr.png"

W, H = 1200, 1680

# ---------------------------------------------------------------------------
# 配色（取自既有 v1.0 海报与图标主题）
# ---------------------------------------------------------------------------
BG_TOP = (7, 23, 47)
BG_BOTTOM = (18, 60, 112)
GRID_MINOR = (38, 86, 140, 55)
GRID_MAJOR = (62, 116, 176, 90)
FRAME = (92, 158, 214, 150)
CYAN = (79, 195, 247, 255)
CYAN_DIM = (86, 200, 245, 150)
ORANGE = (255, 138, 61, 255)
WHITE = (242, 248, 255, 255)
TEXT_LIGHT = (185, 214, 242, 255)
TEXT_DIM = (140, 180, 220, 255)
INK = (15, 42, 78, 255)          # 浅色下载卡片上的墨色
LINK = (11, 87, 164, 255)        # 下载链接
CARD_BG = (234, 243, 255, 255)   # 下载卡片浅底

FONT_DIR = Path("C:/Windows/Fonts")


def font_candidates(*names: str) -> str:
    for n in names:
        p = FONT_DIR / n
        if p.exists():
            return str(p)
    raise RuntimeError(f"未找到可用字体: {names}")


F_CN_BOLD = font_candidates("msyhbd.ttc", "simhei.ttf")
F_CN = font_candidates("msyh.ttc", "simsun.ttc")
F_EN_BOLD = font_candidates("segoeuib.ttf", "arialbd.ttf")
F_MONO = font_candidates("consolab.ttf", "consola.ttf")


def f(path: str, size: int, index: int = 0) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(path, size, index=index)


def text_w(draw: ImageDraw.ImageDraw, s: str, fnt) -> int:
    b = draw.textbbox((0, 0), s, font=fnt)
    return b[2] - b[0]


def centered(draw: ImageDraw.ImageDraw, cy: int, s: str, fnt, fill,
            stroke_width: int = 0, stroke_fill=None, cx: int = W // 2):
    """以 cy 为文字顶部、水平居中绘制。"""
    x = cx - text_w(draw, s, fnt) / 2
    draw.text((x, cy), s, font=fnt, fill=fill,
              stroke_width=stroke_width, stroke_fill=stroke_fill)


def left(draw: ImageDraw.ImageDraw, x: int, y: int, s: str, fnt, fill,
         stroke_width: int = 0, stroke_fill=None):
    draw.text((x, y), s, font=fnt, fill=fill,
              stroke_width=stroke_width, stroke_fill=stroke_fill)


def dashed_line(draw, p1, p2, fill, width=2, dash=16, gap=12):
    x1, y1 = p1
    x2, y2 = p2
    length = math.hypot(x2 - x1, y2 - y1)
    if length == 0:
        return
    ux, uy = (x2 - x1) / length, (y2 - y1) / length
    d = 0.0
    while d < length:
        e = min(d + dash, length)
        draw.line([(x1 + ux * d, y1 + uy * d), (x1 + ux * e, y1 + uy * e)],
                  fill=fill, width=width)
        d += dash + gap


def build_poster(qr_image: Image.Image) -> Image.Image:
    img = Image.new("RGBA", (W, H), (*BG_TOP, 255))
    d = ImageDraw.Draw(img, "RGBA")

    # ---- 背景垂直渐变 ----
    for y in range(H):
        t = y / (H - 1)
        c = tuple(int(BG_TOP[i] + (BG_BOTTOM[i] - BG_TOP[i]) * t) for i in range(3))
        d.line([(0, y), (W, y)], fill=(*c, 255))

    # ---- 工程蓝图网格 ----
    for x in range(0, W + 1, 40):
        d.line([(x, 0), (x, H)], fill=GRID_MAJOR if x % 200 == 0 else GRID_MINOR,
               width=2 if x % 200 == 0 else 1)
    for y in range(0, H + 1, 40):
        d.line([(0, y), (W, y)], fill=GRID_MAJOR if y % 200 == 0 else GRID_MINOR,
               width=2 if y % 200 == 0 else 1)

    # ---- 双线外框（沿用 v1.0 海报）----
    d.rounded_rectangle([(32, 32), (W - 32, H - 32)], radius=6, outline=FRAME, width=2)
    d.rounded_rectangle([(42, 42), (W - 42, H - 42)], radius=4,
                         outline=(92, 158, 214, 70), width=1)

    # ---- 顶部：发布标签 / 主标题 / 中文副标题 ----
    tag_font = f(F_MONO, 21)
    tag_text = "SKETCHCAD  /  RELEASE"
    tw = text_w(d, tag_text, tag_font)
    d.rounded_rectangle([(W / 2 - tw / 2 - 26, 74), (W / 2 + tw / 2 + 26, 118)],
                        radius=22, outline=(79, 195, 247, 190), width=2,
                        fill=(12, 45, 88, 220))
    centered(d, 84, tag_text, tag_font, CYAN)

    title_font = f(F_EN_BOLD, 116)
    # 三层描边：青蓝外发光 → 深蓝实边 → 白色字面
    centered(d, 132, "SketchCAD", title_font, (46, 143, 216, 255),
             stroke_width=10, stroke_fill=(46, 143, 216, 110))
    centered(d, 132, "SketchCAD", title_font, WHITE,
             stroke_width=5, stroke_fill=(6, 26, 56, 255))

    centered(d, 272, "草图设计系统 · 基于 Qt 6 的轻量级 2D CAD 设计软件",
             f(F_CN, 29), TEXT_LIGHT)

    # ---- 版本标识 ----
    centered(d, 322, f"v{VERSION}", f(F_EN_BOLD, 88), CYAN,
             stroke_width=3, stroke_fill=(6, 26, 56, 255))

    rel_font = f(F_MONO, 22)
    rel_text = f"RELEASE TAG   {TAG}"
    rw = text_w(d, rel_text, rel_font)
    d.rounded_rectangle([(W / 2 - rw / 2 - 24, 434), (W / 2 + rw / 2 + 24, 478)],
                        radius=22, outline=(79, 195, 247, 150), width=2,
                        fill=(10, 38, 76, 210))
    centered(d, 443, rel_text, rel_font, TEXT_LIGHT)

    # ---- 中部：抽象 CAD 工程图装饰（设计元素，非软件界面截图）----
    cx, cy, R = W // 2, 700, 190
    dashed_line(d, (110, cy), (W - 110, cy), CYAN_DIM, 2, 18, 14)
    dashed_line(d, (cx, 500), (cx, 900), CYAN_DIM, 2, 18, 14)
    for r, a, wdt in [(R, 120, 2), (158, 170, 2), (96, 210, 2), (46, 235, 2)]:
        d.ellipse([(cx - r, cy - r), (cx + r, cy + r)], outline=(*CYAN[:3], a), width=wdt)
    # 12 边形轮廓（呼应参数化图库的机械零件意象）
    pts = [(cx + R * math.cos(math.radians(15 + i * 30)),
            cy + R * math.sin(math.radians(15 + i * 30))) for i in range(12)]
    d.line(pts + [pts[0]], fill=(*CYAN[:3], 110), width=2)
    for i in range(0, 12):
        a = math.radians(i * 30)
        d.line([(cx + 158 * math.cos(a), cy + 158 * math.sin(a)),
                (cx + R * math.cos(a), cy + R * math.sin(a))],
               fill=(*CYAN[:3], 90), width=2)
    # 橙色中心十字
    d.line([(cx - 20, cy), (cx + 20, cy)], fill=ORANGE, width=3)
    d.line([(cx, cy - 20), (cx, cy + 20)], fill=ORANGE, width=3)
    d.ellipse([(cx - 5, cy - 5), (cx + 5, cy + 5)], outline=ORANGE, width=2)
    # 版本性质标签：加深色底衬，避免文字压在装饰圆线上
    hf = f(F_CN_BOLD, 26)
    ht = "启动故障修复版  /  HOTFIX"
    hw = text_w(d, ht, hf)
    d.rounded_rectangle([(W / 2 - hw / 2 - 24, 906), (W / 2 + hw / 2 + 24, 950)],
                        radius=20, fill=(9, 34, 70, 240),
                        outline=(79, 195, 247, 130), width=1)
    centered(d, 914, ht, hf, CYAN)

    # ---- 版本说明卡片 ----
    d.rounded_rectangle([(60, 962), (W - 60, 1140)], radius=20,
                         fill=(9, 34, 70, 225), outline=(79, 195, 247, 110), width=2)
    d.rounded_rectangle([(60, 962), (68, 1140)], radius=4, fill=(*CYAN[:3], 255))
    left(d, 96, 980, "本版说明", f(F_CN_BOLD, 27), WHITE)
    left(d, 96, 1022, "修复程序启动后主界面尚未显示即退出的问题，主界面恢复正常显示。",
         f(F_CN, 25), TEXT_LIGHT)
    left(d, 96, 1062, f"功能与 v1.1.0 保持一致；同时提供 EXE 与 MSI 两种安装程序。",
         f(F_CN, 25), TEXT_LIGHT)
    chips = ["Windows x64", "Qt 6 + MinGW", "EXE · MSI"]
    chip_x = 96
    for c in chips:
        cf = f(F_MONO, 19)
        cw = text_w(d, c, cf)
        d.rounded_rectangle([(chip_x, 1100), (chip_x + cw + 28, 1130)], radius=15,
                            outline=(79, 195, 247, 130), width=1,
                            fill=(16, 55, 100, 200))
        left(d, chip_x + 14, 1104, c, cf, CYAN)
        chip_x += cw + 40

    # ---- 醒目下载卡片 ----
    d.rounded_rectangle([(60, 1164), (W - 60, 1584)], radius=24,
                        fill=CARD_BG, outline=(79, 195, 247, 255), width=3)
    left(d, 96, 1186, "立即下载", f(F_CN_BOLD, 32), INK)
    right_font = f(F_EN_BOLD, 22)
    right_text = f"DOWNLOAD  v{VERSION}"
    left(d, W - 96 - text_w(d, right_text, right_font), 1195, right_text, right_font,
         (45, 105, 165, 255))

    url_font = f(F_MONO, 24)
    left(d, 96, 1238, DOWNLOAD_URL, url_font, LINK)
    d.line([(96, 1276), (W - 96, 1276)], fill=(150, 190, 230, 255), width=2)

    left(d, 96, 1298, "扫码或访问上方地址，前往 GitHub Release 页面下载",
         f(F_CN, 23), INK)
    left(d, 96, 1340, f"{EXE_NAME}   ·   {MSI_NAME}", f(F_MONO, 19), (45, 105, 165, 255))
    left(d, 96, 1378, "下载后可用随发行版提供的 checksums.sha256 校验完整性",
         f(F_CN, 20), (70, 110, 155, 255))
    left(d, 96, 1418, "EXE 双击安装；MSI 可用命令行静默部署：",
         f(F_CN, 20), (70, 110, 155, 255))
    left(d, 96, 1448, f"msiexec /i {MSI_NAME} /qn", f(F_MONO, 19), (45, 105, 165, 255))
    left(d, 96, 1482, "适用于 Windows 10 / 11 (x64)，安装包已随附 Qt 6 与 MinGW 运行库",
         f(F_CN, 19), (70, 110, 155, 255))

    # 二维码（左下留出文字区，右侧放码）
    qr_size = 232
    qr_x, qr_y = W - 96 - qr_size, 1300
    qr_small = qr_image.resize((qr_size, qr_size), Image.Resampling.LANCZOS)
    d.rounded_rectangle([(qr_x - 10, qr_y - 10), (qr_x + qr_size + 10, qr_y + qr_size + 10)],
                        radius=12, fill=(255, 255, 255, 255),
                        outline=(190, 215, 240, 255), width=2)
    img.paste(qr_small, (qr_x, qr_y))
    qf = f(F_CN, 19)
    qtxt = "扫码直达下载页"
    left(d, qr_x + (qr_size - text_w(d, qtxt, qf)) / 2, qr_y + qr_size + 12,
         qtxt, qf, (45, 105, 165, 255))

    # ---- 底部信息 ----
    d.line([(96, 1600), (W - 96, 1600)], fill=(92, 158, 214, 110), width=1)
    centered(d, 1614,
             f"发布日期 {RELEASE_DATE}     ·     {PUBLISHER}     ·     私有许可证",
             f(F_CN, 21), TEXT_DIM)

    return img.convert("RGB"), (qr_x, qr_y, qr_x + qr_size, qr_y + qr_size)


def make_qr() -> Image.Image:
    qr = qrcode.QRCode(version=None, error_correction=qrcode.constants.ERROR_CORRECT_M,
                       box_size=14, border=4)
    qr.add_data(DOWNLOAD_URL)
    qr.make(fit=True)
    return qr.make_image(fill_color="#0A2148", back_color="white").get_image().convert("RGB")


def decode(img: Image.Image) -> str:
    r = zxingcpp.read_barcode(img)
    return r.text if r else ""


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    qr_img = make_qr()
    qr_img.save(QR_PATH, "PNG")
    assert decode(qr_img) == DOWNLOAD_URL, "二维码内容校验失败（qrcode 产物）"

    # 独立库交叉校验：segno 重新编码同一 URL 后解码，确认内容一致
    segno_path = OUT_DIR / "_qr_crosscheck.png"
    segno.make(DOWNLOAD_URL, error="m").save(str(segno_path), scale=14, border=4,
                                            dark="#0A2148", light="#FFFFFF")
    with Image.open(segno_path) as s:
        assert decode(s.convert("RGB")) == DOWNLOAD_URL, "二维码内容校验失败（segno 交叉校验）"
    segno_path.unlink()

    poster, qr_box = build_poster(qr_img)
    poster.save(POSTER_PATH, "PNG", optimize=True)

    # 从海报成稿上裁下二维码再解码：确认缩小后仍可扫
    with Image.open(POSTER_PATH) as p:
        cropped = p.crop(qr_box).convert("RGB")
    assert decode(cropped) == DOWNLOAD_URL, "海报内二维码缩小后无法解码，请放大二维码尺寸"

    print(f"[OK] 海报       : {POSTER_PATH}  {poster.size[0]}x{poster.size[1]}")
    print(f"[OK] 二维码     : {QR_PATH}  {qr_img.size[0]}x{qr_img.size[1]}")
    print(f"[OK] 编码内容   : {DOWNLOAD_URL}")
    print("[OK] 校验       : qrcode / segno / 海报内缩略码 三处均解码为该 URL")


if __name__ == "__main__":
    main()
