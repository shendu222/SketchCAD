#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 resources/logo.ico（多尺寸应用图标）。

为何不用 Pillow：本机 Python 环境未安装，且为一次性的构建辅助脚本，
不值得为此引入依赖。ICO 允许内嵌 BMP（DIB）格式，本脚本直接手写
BITMAPINFOHEADER + BGRA 像素 + 1bpp AND 掩码，零依赖。

设计要点：
  1) ICO 内可含多张图（16/24/32/48/64/128/256），资源管理器与「属性」页
     会按目标尺寸自动挑选，故必须多尺寸而非单图。
  2) 256×256 用 PNG 压缩可显著减小体积；但 0.3-2.0 版的 PNG 压缩需要 zlib，
     Python 自带 zlib，可直接用。仅 >64px 走 PNG，≤64px 走 BMP/AND 掩码
     （小尺寸下 PNG 反而更大，且部分旧 shell 解析 PNG 帧不佳）。
  3) 图标语义：深蓝底 + 白色 "CAD" 字样 + 青色圆环（呼应 CAD 草图线条）。
"""
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "resources" / "logo.ico"

# 目标尺寸列表（ICO 目录项 + 实际图像）
SIZES = [16, 24, 32, 48, 64, 128, 256]

# ---- 配色（BGR 顺序在下方写入时转换）----
BG_TOP = (0x1B, 0x2E, 0x4F)      # 深蓝顶部
BG_BOT = (0x0D, 0x1A, 0x30)      # 更深的底部
RING = (0x4C, 0xC7, 0xE8)        # 青色圆环
TEXT = (0xFF, 0xFF, 0xFF)        # 白色字
ACCENT = (0xF2, 0xA6, 0x3B)      # 橙色点缀


def lerp(a, b, t):
    return a + (b - a) * t


def render_rgba(n):
    """渲染 n×n 的 RGBA 图像（自绘，5×7 点阵字模画 C/A/D）。"""
    px = [[(0, 0, 0, 0)] * n for _ in range(n)]
    cx = cy = (n - 1) / 2.0
    # 背景：垂直渐变方块（四角切圆，像应用图标而非纯方块）
    radius = n * 0.18
    for y in range(n):
        for x in range(n):
            t = y / max(1, n - 1)
            r = int(lerp(BG_TOP[0], BG_BOT[0], t))
            g = int(lerp(BG_TOP[1], BG_BOT[1], t))
            b = int(lerp(BG_TOP[2], BG_BOT[2], t))
            # 圆角判定（方形边界 + 角半径）
            dx = min(x, n - 1 - x)
            dy = min(y, n - 1 - y)
            if dx < radius and dy < radius:
                ddx = radius - dx
                ddy = radius - dy
                if (ddx * ddx + ddy * ddy) > (radius * radius):
                    continue          # 角外 → 透明
            px[y][x] = (r, g, b, 255)

    # 青色圆环（外圆 r1，内圆 r0）
    r_out = n * 0.40
    r_in = n * 0.345
    for y in range(n):
        for x in range(n):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if r_in <= d <= r_out:
                px[y][x] = (RING[0], RING[1], RING[2], 255)

    # 十字草图线（CAD 草图的十字定位感），画在环内、字下层
    arm = n * 0.26
    thick = max(1, int(round(n * 0.022)))
    for i in range(n):
        for t in range(thick):
            # 水平臂
            xi = int(round(cx - arm)) + i
            if 0 <= xi < n:
                yi = int(round(cy)) + t - thick // 2
                if 0 <= yi < n and px[yi][xi][3] == 255:
                    px[yi][xi] = (ACCENT[0], ACCENT[1], ACCENT[2], 255)
            # 垂直臂
            yi2 = int(round(cy - arm)) + i
            if 0 <= yi2 < n:
                xi2 = int(round(cx)) + t - thick // 2
                if 0 <= xi2 < n and px[yi2][xi2][3] == 255:
                    px[yi2][xi2] = (ACCENT[0], ACCENT[1], ACCENT[2], 255)

    # 5×7 点阵字模绘制 "C" "A" "D"（横向排布，圆环内）
    glyphs = {
        "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
        "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
        "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    }
    order = ["C", "A", "D"]
    gw, gh = 5, 7
    gap = 1
    total_w = len(order) * gw + (len(order) - 1) * gap
    # 字号：随尺寸缩放，小图标保证可读
    scale = max(1, int(round(n * 0.030)))
    if n <= 24:
        scale = 1
    elif n <= 48:
        scale = 2
    elif n <= 128:
        scale = max(2, int(round(n * 0.020)))
    else:
        scale = max(4, int(round(n * 0.018)))
    ox = int(round(cx - (total_w * scale) / 2.0))
    oy = int(round(cy - (gh * scale) / 2.0))
    for gi, ch in enumerate(order):
        rows = glyphs[ch]
        for ry, row in enumerate(rows):
            for rx, bit in enumerate(row):
                if bit != "1":
                    continue
                bx = ox + (gi * (gw + gap) + rx) * scale
                by = oy + ry * scale
                for sy in range(scale):
                    for sx in range(scale):
                        xx, yy = bx + sx, by + sy
                        if 0 <= xx < n and 0 <= yy < n:
                            px[yy][xx] = (TEXT[0], TEXT[1], TEXT[2], 255)
    return px


def make_bmp_dib(n, px):
    """生成 ICO 内嵌的 BMP DIB（含 1bpp AND 掩码，BMP 自下而上）。"""
    # BITMAPINFOHEADER：biHeight = 2*n 表示含 XOR+AND 两层
    hdr = struct.pack(
        "<IiiHHIIiiII",
        40,            # biSize
        n,             # biWidth
        n * 2,         # biHeight（XOR 层 + AND 掩码层）
        1,             # biPlanes
        32,            # biBitCount
        0,             # biCompression = BI_RGB
        n * n * 4,     # biSizeImage（仅 XOR 层）
        0, 0, 0, 0,
    )
    # XOR 层：BGRA，自下而上
    xor = bytearray()
    for y in range(n - 1, -1, -1):
        for x in range(n):
            r, g, b, a = px[y][x]
            xor += bytes((b, g, r, a))
    # AND 掩码：1bpp，每行按 4 字节对齐；位=1 表示透明
    row_bytes = ((n + 31) // 32) * 4
    and_mask = bytearray()
    for y in range(n - 1, -1, -1):
        row = bytearray(row_bytes)
        for x in range(n):
            if px[y][x][3] == 0:
                row[x // 8] |= (0x80 >> (x % 8))
        and_mask += row
    return bytes(hdr) + bytes(xor) + bytes(and_mask)


def make_png(n, px):
    """把 RGBA 编码为 PNG（用于大尺寸，体积更小）。"""
    raw = bytearray()
    for y in range(n):
        raw.append(0)                      # filter type 0 (None)
        for x in range(n):
            r, g, b, a = px[y][x]
            raw += bytes((r, g, b, a))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0)   # 8bit RGBA
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


def main():
    images = []
    for n in SIZES:
        px = render_rgba(n)
        if n >= 128:
            images.append((n, make_png(n, px), True))     # 大尺寸走 PNG
        else:
            images.append((n, make_bmp_dib(n, px), False))
        print(f"  · {n}×{n} {'PNG' if images[-1][2] else 'BMP'}")

    # ICONDIR：6 字节头（reserved, type=1, count）
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    # ICONDIRENTRY：16 字节/项（宽,高,色数,保留,平面,位深,大小,偏移）
    offset = 6 + 16 * len(images)
    blobs = bytearray()
    for n, data, _is_png in images:
        w = 0 if n >= 256 else n
        h = 0 if n >= 256 else n
        out += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(data), offset)
        blobs += data
        offset += len(data)
    out += blobs

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(bytes(out))
    print(f"[OK] 已生成 {OUT}（{len(out)} 字节，{len(images)} 个尺寸）")


if __name__ == "__main__":
    main()
