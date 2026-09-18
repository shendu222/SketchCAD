# -*- coding: utf-8 -*-
"""用第三方解析器 ezdxf 独立校验 SketchCAD 写出的 DXF。

自研的「自己写、自己读」往返测试**无法发现格式不符合外部标准**的问题——
本项目就被 ezdxf 连续暴露出 3 个真实缺陷（多行文本写坏组码、实体与版本不自洽、
R12 不支持 UTF-8 导致中文乱码），因此把第三方校验固化成脚本，每次改动格式层后跑一遍。

用法: python tools/validate_dxf.py [dxf 文件...]
无参数时默认校验 build/ 下的样例（并对中文样例做预期文本比对）。
"""
import os
import re
import sys

try:
    import ezdxf
    from ezdxf import recover
except ImportError:
    print("缺少 ezdxf：请先 pip install ezdxf")
    sys.exit(2)

# (路径, 必须能被第三方解析出来的中文文本；空列表 = 该图应为纯 ASCII)
DEFAULT = [
    ("build/selftest_formats.dxf", []),                                  # 混合图元，纯 ASCII
    ("build/selftest_cn.dxf", ["图纸名称", "图号", "比例", "材料"]),      # 中文图框，GBK / ANSI_936
    ("build/aircraft_preview.dxf", ["侧面图", "俯视图", "正面图"]),       # 飞机三面图，含双行标注
]

fail = 0


def check(cond, msg, detail=""):
    global fail
    print("  [%s] %s%s" % ("PASS" if cond else "FAIL", msg, (" — " + detail) if detail else ""))
    if not cond:
        fail += 1


def declared_codepage(latin1_text):
    """从 HEADER 里取出 $DWGCODEPAGE 的声明值（组码 3）。"""
    m = re.search(r"\$DWGCODEPAGE\s*\r?\n\s*3\s*\r?\n\s*([^\r\n]+)", latin1_text)
    return m.group(1).strip() if m else ""


def validate(path, expect_texts=None):
    print("=" * 68)
    print("校验 %s" % path)
    if not os.path.exists(path):
        check(False, "文件存在")
        return
    # 1) 结构自洽：ezdxf 的 recover 模块做了最严格的 audit（含组码错位、悬空引用）
    try:
        doc, auditor = recover.readfile(path)
    except Exception as e:                                  # noqa: BLE001
        check(False, "能被 ezdxf 解析", repr(e)[:200])
        return
    check(True, "能被 ezdxf 解析")
    check(len(auditor.errors) == 0, "audit 无错误",
          "%d 个: %s" % (len(auditor.errors), auditor.errors[:3]))
    check(len(auditor.fixes) == 0, "audit 无自动修复",
          "%d 个: %s" % (len(auditor.fixes), auditor.fixes[:3]))

    # 2) 版本与实体规模
    ver = doc.dxfversion
    check(ver == "AC1009", "版本为 R12 (AC1009)", "实际=%s" % ver)
    n = len(list(doc.modelspace()))
    check(n > 0, "ENTITIES 段含实体", "%d 个" % n)

    # 3) 编码：R12 不支持 UTF-8，中文必须靠 $DWGCODEPAGE 声明 + 该代码页的字节写出。
    #    判据按「文件实际是否含非 ASCII 字节」分流——纯 ASCII 的图合法地不含中文，
    #    不能一律要求有非 ASCII 字节（早期脚本如此，把 selftest_formats.dxf 误判为失败）。
    raw = open(path, "rb").read()
    declared = declared_codepage(raw.decode("latin-1"))
    has8bit = any(b > 0x7F for b in raw)
    if has8bit:
        check(declared == "ANSI_936",
              "含非 ASCII 字节时声明 $DWGCODEPAGE=ANSI_936", "实际=%r" % declared)
        try:
            raw.decode("gbk")
            gbk_ok = True
        except UnicodeDecodeError:
            gbk_ok = False
        check(gbk_ok, "按 GBK/ANSI_936 可无损解码")
        try:
            raw.decode("utf-8")
            utf8_ok = True
        except UnicodeDecodeError:
            utf8_ok = False
        check(not utf8_ok, "非 ASCII 内容未按 UTF-8 写出（R12 无 UTF-8）")
    else:
        check(declared == "ANSI_1252",
              "纯 ASCII 内容声明 $DWGCODEPAGE=ANSI_1252", "实际=%r" % declared)

    # 4) 组码行/值行严格交替：任何值里出现换行都会让文件错位，ezdxf 会报错，
    #    这里再显式做一遍，方便定位
    txt = raw.decode("gbk" if has8bit else "latin-1", errors="replace")
    t = txt.replace("\r\n", "\n")
    if t.endswith("\n"):
        t = t[:-1]
    lines = t.split("\n")
    strict = len(lines) % 2 == 0
    if strict:
        for i in range(0, len(lines), 2):
            try:
                int(lines[i].strip())
            except ValueError:
                strict = False
                break
    check(strict, "组码/值严格交替（总行数 %d）" % len(lines))

    # 5) 文本实体逐条解码（中文不得出现替换字符）
    bad = 0
    texts = []
    ents = {}
    for e in doc.modelspace():
        ents[e.dxftype()] = ents.get(e.dxftype(), 0) + 1
        if e.dxftype() in ("TEXT", "MTEXT"):
            s = e.dxf.text if e.dxftype() == "TEXT" else e.text
            if "\ufffd" in s:
                bad += 1
            else:
                texts.append(s)
    check(bad == 0, "文字实体无乱码字符", "坏 %d 条" % bad)

    # 6) 最强的一条：第三方解析出的中文必须与预期字面量逐字命中。
    #    为什么不能只查 U+FFFD——GBK 与 UTF-8 的字节被按错误代码页解读时，
    #    产生的仍然是**合法汉字**（只是错的），替换字符检查抓不到这类混写。
    if expect_texts is not None:
        joined = "".join(texts)
        missing = [s for s in expect_texts if s not in joined]
        check(not missing, "第三方解析出的中文与预期逐字一致（未发生编码混写）",
              ("缺失 %s" % missing) if missing else ("命中 %d/%d 条"
                                                    % (len(expect_texts) - len(missing),
                                                       len(expect_texts))))
    if texts:
        print("       示例文本: %s" % " | ".join(repr(s) for s in texts[:4]))
    print("       实体统计: %s" % ", ".join("%s=%d" % kv for kv in sorted(ents.items())))


if __name__ == "__main__":
    args = sys.argv[1:]
    if args:
        for a in args:
            validate(a, None)      # 任意文件：只做结构/编码自洽检查，不猜预期文本
    else:
        for p, expected in DEFAULT:
            validate(p, expected)
    print("=" * 68)
    print("结论: %s" % ("全部通过" if fail == 0 else "%d 项失败" % fail))
    sys.exit(1 if fail else 0)
