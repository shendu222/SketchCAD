# -*- coding: utf-8 -*-
"""用第三方解析器 steputils 独立校验 SketchCAD 写出的 STEP (ISO 10303-21)。

与 validate_dxf.py 同源的动机：自研的「自己写、自己读」往返测试**无法发现不符合外部标准**的
问题。本脚本上线即抓到 4 号真实缺陷——GEOMETRIC_CURVE_SET 的聚合列表带了尾随逗号
（`(#1,#2,)`），ISO 10303-21 明确不允许，严格的解析器直接报
`Unexpected parameter ")" in parameter list`；而自研读入器照收不误。

用法: python tools/validate_step.py [step 文件...]
无参数时默认校验 build/ 下的样例。
"""
import os
import re
import sys

try:
    from steputils import p21
    from steputils.exceptions import ParseError
except ImportError:
    print("缺少 steputils：请先 pip install steputils")
    sys.exit(2)

DEFAULT = [
    "build/selftest_formats.step",
    "build/aircraft_preview.step",
    "build/rt_roundtrip.step",
]

fail = 0


def check(cond, msg, detail=""):
    global fail
    print("  [%s] %s%s" % ("PASS" if cond else "FAIL", msg, (" — " + detail) if detail else ""))
    if not cond:
        fail += 1


def top_entities(text):
    """按顶层 `#id = ENTITY(...);` 切分（不解析参数，仅用于静态检查）。"""
    return re.findall(r"#(\d+)\s*=\s*([A-Z_0-9]+)\s*\(", text)


def validate(path):
    print("=" * 68)
    print("校验 %s" % path)
    if not os.path.exists(path):
        check(False, "文件存在")
        return
    raw = open(path, "rb").read()
    text = raw.decode("utf-8", errors="replace")

    # 0) 静态语法守卫：聚合列表不得有尾随逗号（第三方解析器的报错点，单独立一条便于定位）
    trailing = re.findall(r",\s*\)", text)
    check(not trailing, "聚合列表无尾随逗号（ISO 10303-21 禁止）",
          "发现 %d 处 ',)'" % len(trailing))

    # 1) 独立解析：steputils 是严格的 ISO-10303-21 递归下降解析器
    try:
        with open(path, encoding="utf-8") as fp:
            f = p21.load(fp)
        check(True, "能被 steputils 独立解析（无语法错误）")
    except ParseError as e:
        check(False, "能被 steputils 独立解析（无语法错误）", str(e)[:180])
        return
    except Exception as e:                                  # noqa: BLE001
        check(False, "能被 steputils 独立解析（无语法错误）", repr(e)[:180])
        return

    # 2) 文件结构：HEADER + DATA，且以 END-ISO-10303-21; 收尾（防截断）
    check(text.lstrip().startswith("ISO-10303-21;"), "以 ISO-10303-21; 开头")
    check("END-ISO-10303-21;" in text, "以 END-ISO-10303-21; 收尾（文件未截断）")
    check(text.count("(") == text.count(")"), "括号配平",
          "%d ( vs %d )" % (text.count("("), text.count(")")))

    # steputils: p21.load() → P21File；.data 是 DataSection 的列表，实例在 .instances 字典里。
    # 注意不能直接 list(section)：steputils 的 DataSection.__iter__ 返回了 odict_values 视图
    # 而非迭代器（其自身的小 bug），会被 Python 判为 "iter() returned non-iterator"。
    secs = f.data
    sec = secs[0] if secs else None
    check(sec is not None, "存在 DATA 段")
    ents = list(getattr(sec, "instances", {}).values()) if sec is not None else []
    check(len(ents) > 0, "DATA 段含实体", "%d 个" % len(ents))

    # 3) 语义自洽：曲线集引用的每个 #id 都必须在同一 DATA 段内真实存在且有定义
    defined = set()
    for e in ents:
        ref = str(getattr(e, "ref", "") or "").lstrip("#")
        if ref.isdigit():
            defined.add(int(ref))
    if not defined:                                         # 退化为静态扫描
        defined = {int(m[0]) for m in top_entities(text)}
    dangling = []
    for m in re.finditer(r"GEOMETRIC_CURVE_SET\('SketchCAD',\(([^)]*)\)", text):
        for r in re.findall(r"#(\d+)", m.group(1)):
            if int(r) not in defined:
                dangling.append(r)
    check(not dangling, "曲线集引用的实体全部有定义（无悬空引用）", "悬空 %s" % dangling[:5])

    # 4) 单位与 AP 声明（互操作的关键元数据）
    check("AUTOMOTIVE_DESIGN" in text, "申明 AP214 (AUTOMOTIVE_DESIGN) 模式")
    check("SI_UNIT" in text and ".MILLI." in text, "声明 SI 毫米单位 (.MILLI.,.METRE.)")

    # 5) 实体构成
    from collections import Counter
    c = Counter(m[1] for m in top_entities(text))
    print("       实体统计: %s" % ", ".join("%s=%d" % kv for kv in sorted(c.items(), key=lambda kv: -kv[1])[:8]))


if __name__ == "__main__":
    args = sys.argv[1:]
    for p in (args if args else DEFAULT):
        validate(p)
    print("=" * 68)
    print("结论: %s" % ("全部通过" if fail == 0 else "%d 项失败" % fail))
    sys.exit(1 if fail else 0)
