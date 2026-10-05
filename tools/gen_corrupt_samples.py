#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
确定性损坏样本生成器（鲁棒性 / Fuzz 回归 CI 配套）

用途
----
为 ``t_fileFuzz`` / CI 提供一组**固定种子、可复现**的损坏输入样本，覆盖任务要求的鲁棒性向量：
  * 截断 1% / 10% / 50%   （JSON / DXF / DWG）
  * 随机字节翻转 ≤20 处     （JSON / DXF / DWG）
  * 非法几何 / 结构 / 编码 / 类型键 样本（JSON：type 越界 / 零半径 / 控制字符 / type 键为字符串）

设计要点
--------
  * 种子固定（0x5EED1234），同输入同输出，CI 可反复复跑。
  * DXF / DWG 基准取自 ``tests/golden/golden_r12.{dxf,dwg}``（已签入的有效文件），
    保证「损坏前」本身是合法文件，损坏只来自截断/翻转/注入。
  * JSON 基准由本脚本按固定元素硬编码生成（不依赖外部文件），同样确定性。
  * 输出目录默认 ``tests/golden/corrupt/``，已存在则覆盖式重写。

用法
----
  python3 tools/gen_corrupt_samples.py [--out DIR] [--golden DIR]

退出码：0=成功，非0=前置文件缺失或写盘失败。
"""
import os
import sys
import json
import random
import argparse

SEED = 0x5EED1234
MAX_FLIPS = 20  # 随机字节翻转上限（任务要求 ≤20 处）

# --------------------------------------------------------------------------
# 基准文件
# --------------------------------------------------------------------------
def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()

def write_bytes(path, data):
    with open(path, "wb") as f:
        f.write(data)

def write_text(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)

# --------------------------------------------------------------------------
# 损坏算子（纯字节级，格式无关）
# --------------------------------------------------------------------------
def truncate(data, frac):
    """保留前 (1-frac) 比例字节。"""
    keep = int(len(data) * (1.0 - frac))
    keep = max(0, min(len(data), keep))
    return data[:keep]

def flip(data, n, rng):
    """随机翻转 n 个字节（异或随机非零值）。"""
    if not data or n <= 0:
        return data
    out = bytearray(data)
    n = min(n, len(out))
    for _ in range(n):
        pos = rng.randrange(len(out))
        out[pos] ^= rng.randrange(1, 256)
    return bytes(out)

# --------------------------------------------------------------------------
# 确定性「好」JSON 基准图档（硬编码元素，与 C++ t_fileFuzz 同语义）
# --------------------------------------------------------------------------
def good_json_bytes(rng):
    elements = [
        {"id": 1, "type": 0, "layer": "0",
         "pts": [{"x": 0, "y": 0, "z": 0}, {"x": 10, "y": 0, "z": 0}]},
        {"id": 2, "type": 1, "layer": "0",
         "pts": [{"x": 5, "y": 5, "z": 0}], "radius": 3.0},
        {"id": 3, "type": 2, "layer": "0",
         "pts": [{"x": 0, "y": 0, "z": 0}], "radius": 4.0,
         "startAngle": 0.0, "endAngle": 180.0},
        {"id": 4, "type": 9, "layer": "0", "closed": True,
         "pts": [{"x": 0, "y": 0, "z": 0}, {"x": 10, "y": 0, "z": 0},
                 {"x": 10, "y": 10, "z": 0}, {"x": 0, "y": 10, "z": 0}]},
        {"id": 5, "type": 10, "layer": "0",
         "pts": [{"x": 1, "y": 1, "z": 0}], "text": "标题"},
        {"id": 6, "type": 12, "layer": "0",
         "pts": [{"x": 0, "y": 0, "z": 0}], "blockName": "BOLT"},
        {"id": 7, "type": 11, "layer": "0",
         "pts": [{"x": 0, "y": 0, "z": 0}, {"x": 5, "y": 0, "z": 0},
                 {"x": 5, "y": 5, "z": 0}]},
    ]
    doc = {"elements": elements}
    return json.dumps(doc, ensure_ascii=False, indent=2).encode("utf-8")

# --------------------------------------------------------------------------
# 非法「类型键 / 几何 / 编码」JSON 注入样本
# --------------------------------------------------------------------------
def json_with(elements):
    return json.dumps({"elements": elements}, ensure_ascii=False, indent=2).encode("utf-8")

def illegal_samples(rng):
    """返回 (文件名后缀, 字节) 列表，覆盖：type 越界 / 零半径 / 控制字符 / type 键为字符串。"""
    good = json.loads(good_json_bytes(rng))["elements"]
    out = []

    # type 越界（99）
    e = [dict(x) for x in good]
    e.append({"id": 99, "type": 99, "layer": "0",
              "pts": [{"x": 0, "y": 0, "z": 0}, {"x": 1, "y": 1, "z": 0}]})
    out.append(("type_oob", json_with(e)))

    # 零半径圆（type=1）
    e = [dict(x) for x in good]
    e.append({"id": 100, "type": 1, "layer": "0",
              "pts": [{"x": 0, "y": 0, "z": 0}], "radius": 0.0})
    out.append(("zero_radius", json_with(e)))

    # 控制字符（文本字段含 0x00）
    e = [dict(x) for x in good]
    e.append({"id": 101, "type": 10, "layer": "0",
              "pts": [{"x": 0, "y": 0, "z": 0}], "text": "坏\x00控制字符"})
    out.append(("ctrl_char", json_with(e)))

    # type 键为字符串（应为整数）
    e = [dict(x) for x in good]
    e.append({"id": 102, "type": "Circle", "layer": "0",
              "pts": [{"x": 0, "y": 0, "z": 0}], "radius": 3.0})
    out.append(("type_key_string", json_with(e)))

    return out

# --------------------------------------------------------------------------
# 主流程
# --------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="生成确定性损坏样本")
    ap.add_argument("--out", default=None, help="输出目录（默认 <repo>/tests/golden/corrupt）")
    ap.add_argument("--golden", default=None, help="golden 基准目录（默认 <repo>/tests/golden）")
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    out_dir = args.out or os.path.join(repo, "tests", "golden", "corrupt")
    golden_dir = args.golden or os.path.join(repo, "tests", "golden")
    os.makedirs(out_dir, exist_ok=True)

    rng = random.Random(SEED)

    # ---- DXF / DWG 基准（取自签入的有效 golden）----
    dxf_base_path = os.path.join(golden_dir, "golden_r12.dxf")
    dwg_base_path = os.path.join(golden_dir, "golden_r12.dwg")
    if not os.path.isfile(dxf_base_path):
        print("缺少基准文件：", dxf_base_path, file=sys.stderr); return 2
    if not os.path.isfile(dwg_base_path):
        print("缺少基准文件：", dwg_base_path, file=sys.stderr); return 2

    dxf_base = read_bytes(dxf_base_path)
    dwg_base = read_bytes(dwg_base_path)

    # 截断样本
    for fmt, base in (("dxf", dxf_base), ("dwg", dwg_base)):
        for frac in (0.01, 0.10, 0.50):
            name = "corrupt_%s_trunc_%02d.bin" % (fmt, int(frac * 100))
            write_bytes(os.path.join(out_dir, name), truncate(base, frac))

    # 字节翻转样本（每种格式 6 个，flip 数 1..20 确定性）
    for fmt, base in (("dxf", dxf_base), ("dwg", dwg_base)):
        for i in range(6):
            n = 1 + (rng.randint(0, 19))  # 1..20
            name = "corrupt_%s_flip_%02d.bin" % (fmt, i)
            write_bytes(os.path.join(out_dir, name), flip(base, n, rng))

    # ---- JSON 基准（确定性硬编码）----
    json_base = good_json_bytes(rng)
    write_bytes(os.path.join(out_dir, "base_good.json"), json_base)

    # JSON 截断
    for frac in (0.01, 0.10, 0.50):
        name = "corrupt_json_trunc_%02d.json" % int(frac * 100)
        write_bytes(os.path.join(out_dir, name), truncate(json_base, frac))

    # JSON 字节翻转（6 个）
    for i in range(6):
        n = 1 + rng.randint(0, 19)
        name = "corrupt_json_flip_%02d.json" % i
        write_bytes(os.path.join(out_dir, name), flip(json_base, n, rng))

    # JSON 非法几何/结构/编码/类型键 注入
    for suffix, data in illegal_samples(rng):
        write_bytes(os.path.join(out_dir, "illegal_json_%s.json" % suffix), data)

    print("已生成损坏样本：", out_dir)
    count = len([f for f in os.listdir(out_dir) if not f.startswith(".")])
    print("样本数量：", count)
    return 0

if __name__ == "__main__":
    sys.exit(main())
