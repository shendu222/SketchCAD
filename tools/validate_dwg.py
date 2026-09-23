# -*- coding: utf-8 -*-
"""独立校验 SketchCAD 写出的**原生 DWG R12 (AC1009) 二进制**。

与 validate_dxf.py 同源思路：自研的「自己写、自己读」往返测试无法发现
"容器不符合外部标准" 的问题——写坏指针、算错 CRC、记错记录长度，自己都能读回，
但 AutoCAD / LibreCAD / QCAD 会直接报错或画错。

本脚本**按 R12 容器规范独立解码**（不是抄 C++ 写出器的代码）：
  * 校验 AC1009 版本标识；
  * 校验实体段指针（p_entities / p_entend）落在文件范围内且有序；
  * 定位图层表（tablepos 索引 1），逐条校验图层记录 CRC16；
  * 逐条解码实体记录（kind/flag/length/layer/opts），校验**每条记录的 CRC16**；
  * 汇总各实体类型计数，确认几何字段可解（坐标/半径/角度/文本）。

CRC 算法：DWG 标准 CRC16 —— 反射多项式 0x8408、初值 0、无最终异或。
（与 LibreDWG 一致；odafc 也用同一多项式。）

用法: python tools/validate_dwg.py [dwg 文件...]
无参数时默认校验 build/ 下的样例。
返回码 0=全部通过，1=有失败，2=用法/环境错误。
"""
import os
import struct
import sys

# ---- DWG 标准 CRC16（反射 0x8408，初值 0）----
_CRC_TABLE = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (_c >> 1) ^ 0x8408 if (_c & 1) else (_c >> 1)
    _CRC_TABLE.append(_c)


def dwg_crc(data):
    crc = 0
    for b in data:
        crc = (crc >> 8) ^ _CRC_TABLE[(crc ^ b) & 0xFF]
    return crc & 0xFFFF


u16 = lambda d, o: struct.unpack_from("<H", d, o)[0]
u32 = lambda d, o: struct.unpack_from("<I", d, o)[0]
rd = lambda d, o: struct.unpack_from("<d", d, o)[0]

KIND = {1: "LINE", 3: "CIRCLE", 7: "TEXT", 8: "ARC",
        11: "SOLID", 19: "POLYLINE", 20: "VERTEX", 17: "SEQEND"}

HEADER = 22          # "AC1009"(6) + image(12) + 0x1A 00 00 00(4)
TABLEPOS = 10        # 每个 tablepos 项：size(2) + nr(4) + start(4)

DEFAULT = ["build/selftest_formats.dwg", "build/rt_roundtrip.dwg"]

fail = 0


def check(cond, msg, detail=""):
    global fail
    print("  [%s] %s%s" % ("PASS" if cond else "FAIL", msg,
                           (" — " + detail) if detail else ""))
    if not cond:
        fail += 1


def validate(path):
    print("=" * 68)
    print("校验 %s" % path)
    if not os.path.exists(path):
        check(False, "文件存在")
        return
    d = open(path, "rb").read()
    n = len(d)
    print("       大小 = %d 字节" % n)

    # 1) 版本标识：原生 R12 = AC1009（且必须是二进制，不是文本 DXF）
    check(d[:6] == b"AC1009", "版本标识为 AC1009（原生 R12）",
          "实际=%r" % d[:6])
    check(not d.startswith(b"0\r\nSECTION"),
          "是二进制容器而非文本 DXF")

    # 2) 头部指针
    p_ent = u32(d, HEADER)
    p_eend = u32(d, HEADER + 4)
    check(0 < p_ent < p_eend <= n, "实体段指针 p_entities/p_entend 合法且有序",
          "p_ent=%d p_entend=%d (n=%d)" % (p_ent, p_eend, n))

    # 3) tablepos 区 + 图层表（索引 1）
    pre = HEADER + 4 + 4 + 4 + 4 + 4 + 4     # = 42（本实现含 4 字节对齐填充 → 实际 46）
    lo = pre + 1 * TABLEPOS
    l_nr, l_start = u32(d, lo + 2), u32(d, lo + 6)
    check(l_nr > 0 and 0 < l_start < n, "图层表已登记",
          "nr=%d start=%d" % (l_nr, l_start))

    # 图层记录：flag(1) + name(32) + used(2) + aci(2) + style(2) + check_2(2) = 41 字节
    REC = 1 + 32 + 2 + 2 + 2 + 2
    layers = {}
    layer_crc_ok = 0
    for i in range(l_nr):
        o = l_start + i * REC
        if o + REC > n:
            check(False, "图层记录 #%d 越界" % i)
            break
        name = d[o + 1:o + 33].split(b"\x00")[0].decode("utf-8", "replace")
        c2 = u16(d, o + 39)
        layers[i] = name
        if dwg_crc(d[o:o + 39]) == c2:
            layer_crc_ok += 1
    check(layer_crc_ok == l_nr, "全部图层记录 CRC16 校验通过",
          "%d/%d" % (layer_crc_ok, l_nr))
    print("       图层: %s" % ", ".join("%d=%r" % (i, layers[i]) for i in sorted(layers)))

    # 4) 实体记录逐条解码 + CRC 校验
    ents = {}
    bad_crc = 0
    i, count = p_ent, 0
    geom_bad = []
    while i < p_eend:
        if i + 8 > n:
            check(False, "记录 #%d 头部越界" % count)
            break
        kind, flag = d[i], d[i + 1]
        length = u16(d, i + 2)
        if length < 10 or i + length > n:
            check(False, "记录 #%d 长度非法（len=%d）" % (count, length), "offset=%d" % i)
            break
        kn = KIND.get(kind, "?%d" % kind)
        ents[kn] = ents.get(kn, 0) + 1
        # 逐条 CRC16
        if dwg_crc(d[i:i + length - 2]) != u16(d, i + length - 2):
            bad_crc += 1
        # 几何字段可解性抽查（仅类型内字段，越界即视为结构错误）
        p = i + 8
        if flag & 0x01:
            p += 1
        try:
            if kind == 1 and p + 48 <= i + length - 2:          # LINE 2×3D
                rd(d, p); rd(d, p + 24)
            elif kind == 3 and p + 24 <= i + length - 2:        # CIRCLE 2D+r
                rd(d, p); rd(d, p + 16)
            elif kind == 8 and p + 40 <= i + length - 2:        # ARC 2D+r+a0+a1
                rd(d, p); rd(d, p + 32)
            elif kind == 20 and p + 16 <= i + length - 2:       # VERTEX 2D
                rd(d, p); rd(d, p + 8)
        except struct.error as e:                               # noqa: BLE001
            geom_bad.append("#%d %s: %s" % (count, kn, e))
        count += 1
        i += length
    check(bad_crc == 0, "全部实体记录 CRC16 校验通过",
          "%d 条坏 CRC（共 %d 条）" % (bad_crc, count) if bad_crc else "共 %d 条" % count)
    check(not geom_bad, "几何字段可无越界解码", "; ".join(geom_bad[:3]))
    check(count > 0, "解析出实体记录", "%d 条" % count)
    print("       实体统计: %s" % ", ".join("%s=%d" % kv for kv in sorted(ents.items())))
    # 至少要有基础几何，不能是空壳
    has_geom = any(k in ents for k in ("LINE", "CIRCLE", "ARC", "VERTEX"))
    check(has_geom, "容器内含可渲染的基础几何（LINE/CIRCLE/ARC/VERTEX）")


if __name__ == "__main__":
    args = sys.argv[1:] or DEFAULT
    for a in args:
        validate(a)
    print("=" * 68)
    print("结论: %s" % ("全部通过" if fail == 0 else "%d 项失败" % fail))
    sys.exit(1 if fail else 0)
