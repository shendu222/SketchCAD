# -*- coding: utf-8 -*-
"""用 ezdxf（第三方实现）生成「外部来源」的 DXF 样例，供 SketchCAD 的导入路径做互操作测试。

**为什么需要这个脚本**：本项目的往返测试一直是「自己写、自己读」，那只验证了内部自洽；
写入端已经有 4 个真实缺陷是第三方解析器抓到的。读入端同样危险——它至今只吃过自己写的文件，
而本程序写出时会用一些**自己顺手**的写法（坐标 6 位小数、CRLF、自己定义的图层名、
ARC 角度直接写度数……）。真实世界里的 DXF 由 AutoCAD / LibreCAD / ezdxf 写出，
在这些维度上未必一致。所以需要一份**别人写的**文件来打读入端。

生成物（both 提交进仓库，供 C++ 测试离线读取，不依赖 Python）：
  tests/data/foreign_ezdxf_r12.dxf    ezdxf 写出的 R12，GBK 中文，CRLF
  tests/data/foreign_lf_only.dxf      同上内容但**仅 LF 换行**（对抗性：很多工具这么写）

用法: python tools/make_foreign_dxf.py
"""
import io
import os
import sys

try:
    import ezdxf
except ImportError:
    print("缺少 ezdxf：请先 pip install ezdxf")
    sys.exit(2)

OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tests", "data")

# 期望值（供 C++ 测试断言；改这里必须同步改 tests/dxf_import_test.cpp）
EXPECT = ""


def build():
    doc = ezdxf.new("R12")
    # 强制 GBK：R12 的字符编码由 $DWGCODEPAGE 声明，cjk 内容必须走 ANSI_936。
    # 注意：直接写 doc.header["$DWGCODEPAGE"] 会被 ezdxf 在写出时按 doc.encoding 覆盖，
    # 必须改 doc.encoding，否则会产出「声明 ANSI_1252、字节却是 GBK」的不一致文件。
    doc.encoding = "gbk"

    for name, color in (("OUTLINE", 1), ("CENTER", 4), ("NOTES", 7)):
        if name not in doc.layers:
            doc.layers.add(name, color=color)

    msp = doc.modelspace()
    # 1) 直线（OUTLINE 图层）
    msp.add_line((0, 0), (100, 0), dxfattribs={"layer": "OUTLINE"})
    # 2) 圆（CENTER 图层）
    msp.add_circle((50, 50), 20, dxfattribs={"layer": "CENTER"})
    # 3) 圆弧：圆心 (10,10) R5，10°→80°
    msp.add_arc((10, 10), 5, 10, 80, dxfattribs={"layer": "OUTLINE"})
    # 4) 多段线（闭合）：必须显式给 format="xyseb"（x, y, startW, endW, bulge），
    #    bulge 写在**段起点**。90° 圆弧的 bulge = tan(90°/4) = 0.41421356，
    #    与 SketchCAD 的导出写法一致，可直接用于「读入外部 bulge」的断言。
    msp.add_polyline2d(
        [(200, 0, 0, 0, 0.41421356), (240, 0, 0, 0, 0), (240, 40, 0, 0, 0), (200, 40, 0, 0, 0)],
        format="xyseb",
        close=True,
        dxfattribs={"layer": "OUTLINE"},
    )
    # 5) 中文文本（验证 GBK 读入）
    msp.add_text("外部样例图", height=5.0,
                 dxfattribs={"layer": "NOTES"}).set_placement((0, -20))
    # 6) 纯数字文本
    msp.add_text("R12 IMPORT", height=3.0,
                 dxfattribs={"layer": "NOTES"}).set_placement((0, -30))

    return doc


def write(doc, path, lf_only):
    buf = io.StringIO()
    doc.write(buf)
    text = buf.getvalue()
    text = text.replace("\r\n", "\n")
    if not lf_only:
        text = text.replace("\n", "\r\n")     # 模拟 AutoCAD 习惯写法
    with open(path, "wb") as fp:
        fp.write(text.encode("gbk"))          # 中文 → GBK 字节
    raw = open(path, "rb").read()
    return len(raw), raw.count(b"\r\n"), raw.count(b"\n")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    doc = build()
    for name, lf in (("foreign_ezdxf_r12.dxf", False), ("foreign_lf_only.dxf", True)):
        path = os.path.join(OUT_DIR, name)
        size, crlf, lf_n = write(doc, path, lf)
        print("已写出 %-28s %6d 字节  CRLF=%-4d  LF总数=%d" % (name, size, crlf, lf_n))
    # 自检：确认 ezdxf 自己能读回来（保证样例本身不是坏文件）
    for name in ("foreign_ezdxf_r12.dxf", "foreign_lf_only.dxf"):
        path = os.path.join(OUT_DIR, name)
        d2 = ezdxf.readfile(path)
        ents = list(d2.modelspace())
        kinds = {}
        for e in ents:
            kinds[e.dxftype()] = kinds.get(e.dxftype(), 0) + 1
        print("  回读 %-28s 实体=%d  %s" % (name, len(ents), kinds))


if __name__ == "__main__":
    main()
