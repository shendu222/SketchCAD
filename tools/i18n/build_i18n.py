# -*- coding: utf-8 -*-
"""
SketchCAD 英文界面「零中文残留」生成器 / 校验器
=============================================================================
用法（在 sketch-cad-cpp/ 根目录执行）：
    python tools/i18n/build_i18n.py --check     # 只校验，不写文件
    python tools/i18n/build_i18n.py --apply     # 生成词典 + 包装源码 + 扩展 .ts

职责：
  1. 扫描 src/** 的中文字符串字面量，只挑「用户可见文本出口」（菜单/标签/按钮/
     弹窗/状态/空态/命令行反馈/动态模板等调用点），生成中文源串全集。
  2. 用 en_phrases.txt（逐条人工校准）+ en_glossary.txt（CAD/UI 术语表，最长匹配）
     为每个源串产出英文，保证译文不含任何 CJK 字符，且 %1..%9 占位符数量一致。
  3. 生成 src/core/I18nDict.cpp（raw string 数据块 + 惰性 QHash 解析，规避
     MinGW GCC 13.1 巨型聚合初始化静默崩溃）。
  4. 把上述字面量在源码里包装成 I18n::t(...)：中文模式恒等返回，英文模式查表翻译。
     非 UI 上下文（比较键、JSON 键、字体族名、文件名、文档内容）一律不包装。
  5. 扩展 resources/translations/sketchcad_en.ts，补齐所有 tr() 源串译文。
  6. 输出可复核的覆盖率/残留报告 tools/i18n/i18n-report.txt。

设计底线：中文界面逐字不变；命令名/别名/JSON 键/字体族名/设置键/文档内容不进词典。
"""
import argparse
import collections
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "src")
TS = os.path.join(ROOT, "resources", "translations", "sketchcad_en.ts")
DICT_CPP = os.path.join(ROOT, "src", "core", "I18nDict.cpp")
REPORT = os.path.join(ROOT, "tools", "i18n", "i18n-report.txt")
PHRASES = os.path.join(ROOT, "tools", "i18n", "en_phrases.txt")
GLOSSARY = os.path.join(ROOT, "tools", "i18n", "en_glossary.txt")
# 逐条人工英译的翻译记忆（JSON dict：中文源串 -> 英文），优先于术语组合。
TRANSLATIONS_JSON = os.path.join(ROOT, "tools", "i18n", "en_translations.json")
# 在统一反馈出口（而非字面量处）翻译的中文串：源码里以 const char* 传递，
# 无法自动反查，需显式登记，否则英文模式这些错误反馈会漏译。
SOURCES_EXTRA = os.path.join(ROOT, "tools", "i18n", "en_sources_extra.txt")
# 已包装过的中文源串清单：包装后字面量本身已变成英文出口，重新扫描无法再发现，
# 因此用持久化清单保证「再次运行生成器」时词典不缩水、可增量追加新文案。
SOURCES_JSON = os.path.join(ROOT, "tools", "i18n", "en_sources.json")

CJK = re.compile(r"[\u3400-\u9fff]")
# 中文标点/全角符号也不能留在英文界面（不含技术符号 ×²Ø→° 等）。
CJK_PUNCT = re.compile(r"[\u3000-\u303f\uff01-\uff60\uffe0-\uffe6「」『』《》【】、。！？：；，]")
PURE_CJK = re.compile(r"^[\u3400-\u9fff]+$")
STR_RE = re.compile(r'(QStringLiteral\(\s*)?"((?:[^"\\]|\\.)*)"')
PLACEHOLDER = re.compile(r"%\d+")

# 这些函数是「字符串构造器」，不算文本出口；需要继续向外找真正的调用者。
WRAPPERS = {
    "QStringLiteral", "QString", "tr", "arg", "QLatin1String",
    "QString::fromUtf8", "QString::fromLocal8Bit", "QObject::tr",
    "QCoreApplication::translate", "QString::number", "I18n::t",
    "I18n::tUtf8", "I18n::termScrub",
}

# 用户可见文本出口（函数名 / 类名构造器）。
EMITTERS = set("""
log setText setWindowTitle setToolTip setStatusTip setWhatsThis setPlaceholderText
showMessage addAction addMenu addItem addItems insertItem insertItems
setHeaderLabels setHorizontalHeaderLabels setVerticalHeaderLabels setHeaderData
setTabText setItemText setTitle setLabel append appendPlainText appendHtml
setPlainText setHtml setInformativeText setDetailedText setTextInteractionFlags
addRow addTab registerTool mk mkLine mkCircle mkArc mkText mkMText mkPoly
createDropdownTool act pushMemento fail setErr appendResult appendLog
labelInCell beginEdit addD addComboItem addSubMenu addEx mkLine createSolid
setButtonText setDefaultText setIconText setStatusTip setSectionText
setMessage setCaption setBanner setEmptyText setPlaceholderContent
showText drawText drawTextItem setHeaderLabel setItemDelegate
QMessageBox::warning QMessageBox::information QMessageBox::critical
QMessageBox::question QMessageBox::about QMessageBox::aboutQt
QInputDialog::getText QInputDialog::getInt QInputDialog::getDouble
QInputDialog::getItem QInputDialog::getMultiLineText
QFileDialog::getSaveFileName QFileDialog::getOpenFileName
QFileDialog::getExistingDirectory QFileDialog::getOpenFileNames
QToolTip::showText
QLabel QPushButton QCheckBox QRadioButton QGroupBox QMenu QAction QDockWidget
QTreeWidgetItem QTableWidgetItem QStandardItem QProgressDialog QComboBox
QLineEdit QTextEdit QPlainTextEdit QSpinBox QDoubleSpinBox QToolButton
QTabWidget QGraphicsSimpleTextItem QGraphicsTextItem QListWidgetItem
QMessageBox QInputDialog QFileDialog QErrorMessage QWizardPage
""".split())

# 这些文件里的中文属于「图纸文档内容」或「内部数据」，不是界面文案：
# 标题栏单元格文字、视图标识、材料/字体名、图层名、剖面标记等。
# 翻译它们会改变保存的 .sketch/.dxf 内容，属于与翻译无关的行为变化，故整体跳过。
SKIP_FILES = {
    "src/core/sheet_frame.cpp",
    "src/core/SheetFrameTemplate.cpp",
    "src/core/TitleBlockLayout.cpp",
    "src/core/PartLibrary.cpp",
    "src/core/Section.cpp",
    "src/core/CenterLineGen.cpp",
    "src/core/SymbolFactory.cpp",
    "src/core/ProjectionSymbol.cpp",
    "src/core/SurfaceRoughness.cpp",
    "src/core/Gdt.cpp",
    "src/core/weld/WeldSymbol.cpp",
    "src/core/InvoluteGear.cpp",
    "src/core/GbFastenerTable.cpp",
    "src/core/HatchLibrary.cpp",
    "src/core/LinePattern.cpp",
    "src/core/LineStyleConfig.cpp",
    "src/core/CadFormats.cpp",
    "src/core/TextStyle.cpp",
    "src/core/PlotStyle.cpp",
    "src/core/ZwcadTextLayout.cpp",
    "src/core/Element.cpp",
    "src/core/Element.h",
    "src/core/I18n.cpp",
    "src/core/I18nDict.cpp",
    "src/core/standards/StandardsDB.cpp",
}

# 元数据表在单一出口统一翻译，不逐条包装字面量（避免 const char* 形参编译错误）。
SINK_FILES = {
    "src/core/CommandCatalog.cpp",
    "src/core/ToolRegistry.cpp",
}

# 明确不进词典的内部字符串（即使出现在出口上下文，也必须保持中文原样）。
EXCLUDE_STRINGS = {
    "长仿宋体", "长仿宋", "仿宋", "宋体", "黑体", "楷体",
    "CAD封面.jpeg", "CAD封面.jpg", "CAD封面.png",
}

# ---------------------------------------------------------------------------
# C++ 字符串字面量转义 / 反转义
# ---------------------------------------------------------------------------
def unescape(s):
    """把源码字面量内容还原成运行时字符串。"""
    out = []
    i = 0
    simple = {"n": "\n", "t": "\t", "r": "\r", "0": "\0",
              "\\": "\\", '"': '"', "'": "'", "a": "\a", "b": "\b",
              "f": "\f", "v": "\v"}
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            c = s[i + 1]
            if c == "x":
                m = re.match(r"[0-9a-fA-F]+", s[i + 2:])
                if m:
                    out.append(chr(int(m.group(0), 16)))
                    i += 2 + len(m.group(0))
                    continue
            if c == "u" and i + 6 <= len(s):
                try:
                    out.append(chr(int(s[i + 2:i + 6], 16)))
                    i += 6
                    continue
                except ValueError:
                    pass
            out.append(simple.get(c, c))
            i += 2
            continue
        out.append(s[i])
        i += 1
    return "".join(out)


def escape(s):
    """把运行时字符串编码成 C++ 普通字符串字面量内容。"""
    return (s.replace("\\", "\\\\").replace('"', '\\"')
             .replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")
             .replace("\0", "\\0"))


# ---------------------------------------------------------------------------
# 词典
# ---------------------------------------------------------------------------
def load_pairs(path):
    pairs = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.lstrip().startswith("#"):
                continue
            if "=" not in line:
                continue
            # 键/值都可能含等号（如模板串「已创建%1 #%2 = %3 mm（%4）」）：
            # 按最后一个等号切分，键含等号不错位，值含等号原样保留。
            k, v = line.rsplit("=", 1)
            k, v = k.strip(), v.strip()
            # 首次出现优先：文件按「领域术语 → UI 外壳 → 状态短语 → 语法词 →
            # 单字兜底」排序，前面的专业释义不能被后面的泛化单字覆盖。
            if k and k not in pairs:
                pairs[k] = v
    return pairs


PHRASE_MAP = load_pairs(PHRASES)
# 全量人工英译（翻译记忆）：逐条优先；手工短语表保留在前面做局部微调。
if os.path.exists(TRANSLATIONS_JSON):
    import json as _json
    with open(TRANSLATIONS_JSON, encoding="utf-8") as _f:
        for _k, _v in _json.load(_f).items():
            PHRASE_MAP.setdefault(_k, _v)
GLOSSARY_MAP = load_pairs(GLOSSARY)
TERM_MAP = {k: v for k, v in GLOSSARY_MAP.items() if PURE_CJK.match(k)}
MAX_TERM_LEN = max((len(k) for k in TERM_MAP), default=1)

PUNCT = {
    "，": ", ", "。": ". ", "；": "; ", "：": ": ", "、": ", ",
    "！": "! ", "？": "? ", "（": " (", "）": ") ",
    "「": ' "', "」": '" ', "『": ' "', "』": '" ',
    "《": ' "', "》": '" ', "【": " [", "】": "] ",
    "…": "...", "—": " - ", "·": " ", "％": "%",
    "＋": "+", "－": "-", "＝": "=", "／": "/", "＼": "\\",
    "＜": "<", "＞": ">", "［": "[", "］": "]",
}


def tidy(text, capitalize=True):
    """英文成串后的规整：空白、标点、重复词、大小写。"""
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s+([,.;:!?\]})])", r"\1", text)
    text = re.sub(r"([(\[{])\s+", r"\1", text)
    text = re.sub(r"(%\d+)\s+", r"\1 ", text)
    text = re.sub(r"\s+(%\d+)", r" \1", text)
    # 相邻重复词（如 "compatible compatible" / "connect connect"）
    text = re.sub(r"\b([A-Za-z]{3,})\s+\1\b", r"\1", text, flags=re.I)
    text = re.sub(r"\s+", " ", text).strip()
    # 首字母大写；&助记符、占位符、符号开头、全大写缩写开头保持原样
    if capitalize and text and not re.match(r"^[&%(\[⚠·\-0-9]", text):
        if not (len(text) > 1 and text[:3].isupper()):
            text = text[0].upper() + text[1:]
    return text


def compose(zh, allow_single=True):
    """术语表最长匹配翻译；返回不含 CJK 的英文。
    长句（>6 个汉字）默认不启用单字兜底，避免把语法字拼成噪声英文。"""
    if not CJK.search(zh):
        return zh                      # 纯 ASCII（命令语法/占位符）逐字保留
    capitalize = bool(CJK.match(zh[0]))   # 以 ASCII 命令名开头时不得改其大小写
    out = []
    i, n = 0, len(zh)
    cjk_count = len(CJK.findall(zh))
    min_len = 1 if (allow_single and cjk_count <= 6) else 2
    while i < n:
        ch = zh[i]
        if CJK.match(ch):
            hit = False
            for L in range(min(MAX_TERM_LEN, n - i), min_len - 1, -1):
                w = zh[i:i + L]
                if w in TERM_MAP:
                    val = TERM_MAP[w].strip()
                    if val:
                        out.append(" " + val + " ")
                    i += L
                    hit = True
                    break
            if hit:
                continue
            # 术语表覆盖不到的汉字直接省略（绝不把汉字写进译文）
            out.append(" ")
            i += 1
            continue
        out.append(PUNCT.get(ch, ch))
        i += 1
    return tidy("".join(out), capitalize=capitalize)


# 句式模板：把高频反馈/提示句式整体改写成自然英文，捕获组递归做术语级翻译。
# {1}..{3} 为捕获组占位；组内容仍走 compose()，ASCII 命令语法原样保留。
PATTERNS = [
    (r"^用法[：:]\s*(.*)$", "Usage: {1}"),
    (r"^示例[：:]\s*(.*)$", "Example: {1}"),
    (r"^提示[：:]\s*(.*)$", "Hint: {1}"),
    (r"^错误[：:]\s*(.*)$", "Error: {1}"),
    (r"^警告[：:]\s*(.*)$", "Warning: {1}"),
    (r"^说明[：:]\s*(.*)$", "Description: {1}"),
    (r"^参数[：:]\s*(.*)$", "Parameters: {1}"),
    (r"^注意[：:]\s*(.*)$", "Note: {1}"),
    (r"^结论[：:]\s*(.*)$", "Conclusion: {1}"),
    (r"^建议[：:]\s*(.*)$", "Suggestion: {1}"),
    (r"^原因[：:]\s*(.*)$", "Reason: {1}"),
    (r"^结果[：:]\s*(.*)$", "Result: {1}"),
    (r"^状态[：:]\s*(.*)$", "Status: {1}"),
    (r"^请先选中(.+)$", "Please select {1} first"),
    (r"^请先(.+)$", "Please {1} first"),
    (r"^请选择(.+)$", "Please select {1}"),
    (r"^请输入(.+)$", "Please enter {1}"),
    (r"^请指定(.+)$", "Please specify {1}"),
    (r"^请(.+)$", "Please {1}"),
    (r"^无法(.+)$", "Cannot {1}"),
    (r"^不能(.+)$", "Cannot {1}"),
    (r"^不支持(.+)$", "Not supported: {1}"),
    (r"^未找到(.+)$", "{1} not found"),
    (r"^找不到(.+)$", "{1} not found"),
    (r"^(.+?)不存在$", "{1} does not exist"),
    (r"^(.+?)解析失败$", "Failed to parse {1}"),
    (r"^(.+?)无效$", "Invalid {1}"),
    (r"^无效的(.+)$", "Invalid {1}"),
    (r"^已创建(.+)$", "Created {1}"),
    (r"^已删除(.+)$", "Deleted {1}"),
    (r"^已清除(.+)$", "Cleared {1}"),
    (r"^已生成(.+)$", "Generated {1}"),
    (r"^已保存(.+)$", "Saved {1}"),
    (r"^已加载(.+)$", "Loaded {1}"),
    (r"^已选中(.+)$", "Selected {1}"),
    (r"^已将(.+)$", "Applied {1}"),
    (r"^已(.+)$", "Completed: {1}"),
    (r"^无可(.+)$", "No {1} available"),
    (r"^(.+)失败$", "Failed to {1}"),
    (r"^(.+)不能为空$", "{1} cannot be empty"),
    (r"^(.+)须为正数$", "{1} must be positive"),
    (r"^(.+)须为正$", "{1} must be positive"),
]
PATTERNS = [(re.compile(p), t) for p, t in PATTERNS]


def apply_patterns(zh):
    for rx, tpl in PATTERNS:
        m = rx.match(zh)
        if not m:
            continue
        groups = [compose(g, allow_single=True) if g else "" for g in m.groups()]
        out = tpl
        for idx, g in enumerate(groups, 1):
            out = out.replace("{%d}" % idx, g)
        if "{" in out and re.search(r"\{\d+\}", out):
            continue
        return tidy(out)
    return None


def translate(zh):
    if zh in PHRASE_MAP:
        return PHRASE_MAP[zh]
    shaped = apply_patterns(zh)
    if shaped is not None:
        return shaped
    return compose(zh)


# ---------------------------------------------------------------------------
# 源码扫描：定位「用户可见文本出口」里的中文字符串字面量
# ---------------------------------------------------------------------------
def stack_map(s):
    """每个字符串起始双引号位置 -> 当时的调用栈（函数名列表）。"""
    n = len(s)
    stack, pos2, i = [], {}, 0
    while i < n:
        c = s[i]
        if c == '"':
            pos2[i] = [x[1] for x in stack]
            j = i + 1
            while j < n:
                if s[j] == "\\":
                    j += 2
                    continue
                if s[j] == '"':
                    break
                j += 1
            i = j + 1
            continue
        if c == "'":
            j = i + 1
            while j < n:
                if s[j] == "\\":
                    j += 2
                    continue
                if s[j] == "'":
                    break
                j += 1
            i = j + 1
            continue
        if c == "/" and i + 1 < n and s[i + 1] == "/":
            j = s.find("\n", i)
            i = j if j >= 0 else n
            continue
        if c == "/" and i + 1 < n and s[i + 1] == "*":
            j = s.find("*/", i + 2)
            i = j + 2 if j >= 0 else n
            continue
        if c == "(":
            k = i - 1
            while k >= 0 and (s[k].isalnum() or s[k] in "_:>"):
                k -= 1
            stack.append((i, s[k + 1:i].strip()))
            i += 1
            continue
        if c == ")":
            if stack:
                stack.pop()
            i += 1
            continue
        i += 1
    return pos2


def outer_call(s, qpos, pos2):
    st = pos2.get(qpos, [])
    for nm in reversed(st):
        if nm in WRAPPERS:
            continue
        return nm.lstrip(">")
    return "<none>"


def iter_sources():
    for dp, dn, fn in os.walk(SRC):
        for f in fn:
            if not f.endswith((".cpp", ".h")):
                continue
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, ROOT).replace("\\", "/")
            yield rel, p


def comment_mask(s):
    """标记注释里的字符位置，避免把注释中的中文串误当成 UI 文案。"""
    n = len(s)
    mask = bytearray(n)
    i = 0
    while i < n:
        if s[i] == '"':
            j = i + 1
            while j < n:
                if s[j] == "\\":
                    j += 2
                    continue
                if s[j] == '"':
                    break
                j += 1
            i = j + 1
            continue
        if s[i] == "/" and i + 1 < n and s[i + 1] == "/":
            j = s.find("\n", i)
            j = n if j < 0 else j
            mask[i:j] = b"\x01" * (j - i)
            i = j
            continue
        if s[i] == "/" and i + 1 < n and s[i + 1] == "*":
            j = s.find("*/", i + 2)
            j = n if j < 0 else j + 2
            mask[i:j] = b"\x01" * (j - i)
            i = j
            continue
        i += 1
    return mask


def only_trivia(gap):
    g = re.sub(r"/\*.*?\*/", "", gap, flags=re.S)
    g = re.sub(r"//[^\n]*", "", g)
    return g.strip() == ""


def scan():
    """返回 [(rel, path, source, [(start, end, joined_runtime_string, outer_call)])]。

    C++ 相邻字符串字面量（`"A" "B"`）会被编译器自动拼接，必须作为一个整体
    翻译单元处理：逐段包装会生成 `t("A") t("B")` 这种非法表达式。
    """
    result = []
    for rel, p in iter_sources():
        if rel in SKIP_FILES or rel in SINK_FILES:
            continue
        with open(p, encoding="utf-8", errors="replace") as f:
            s = f.read()
        mask = comment_mask(s)
        p2 = stack_map(s)
        matches = [m for m in STR_RE.finditer(s) if not mask[m.start()]]

        # 把「仅由空白/注释分隔」的相邻字面量聚类成一个翻译单元
        runs, cur = [], []
        for m in matches:
            if cur and only_trivia(s[cur[-1].end():m.start()]):
                cur.append(m)
            else:
                if cur:
                    runs.append(cur)
                cur = [m]
        if cur:
            runs.append(cur)

        hits = []
        for run in runs:
            first, last = run[0], run[-1]
            q0 = first.start(2) - 1
            # 已包装过 → 幂等跳过
            tail = s[max(0, q0 - 40):q0]
            if re.search(r"I18n::t\(\s*(?:(?:QStringLiteral|QString::fromUtf8)\(\s*)?$", tail):
                continue
            # tr("中文") 由 tr 包装器单独整体处理，避免区间重叠
            if any(s[max(0, (mm.start() if mm.group(1) else mm.start(2) - 1) - 3):
                      (mm.start() if mm.group(1) else mm.start(2) - 1)] == "tr("
                   for mm in run):
                continue
            joined = "".join(unescape(mm.group(2)) for mm in run)
            if not CJK.search(joined) or joined in EXCLUDE_STRINGS:
                continue
            if "\x00" in joined:
                continue
            oc = outer_call(s, q0, p2)
            if oc not in EMITTERS:
                continue
            # 若整段以 QStringLiteral( 开头，须把它的右括号一起纳入替换范围
            expr_end = last.end()
            if first.group(1):
                k = last.end()
                while k < len(s) and s[k].isspace():
                    k += 1
                if k < len(s) and s[k] == ")":
                    expr_end = k + 1
            hits.append((first.start(), expr_end, joined, oc))
        if hits:
            result.append((rel, p, s, hits))
    return result


def sink_strings():
    """元数据表（CommandCatalog / ToolRegistry）里的全部中文串。"""
    out = set()
    for rel in SINK_FILES:
        p = os.path.join(ROOT, rel)
        with open(p, encoding="utf-8") as f:
            s = f.read()
        for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', s):
            zh = unescape(m.group(1))
            if CJK.search(zh) and zh not in EXCLUDE_STRINGS:
                out.add(zh)
        for m in re.finditer(r'QStringLiteral\("((?:[^"\\]|\\.)*)"\)', s):
            zh = unescape(m.group(1))
            if CJK.search(zh) and zh not in EXCLUDE_STRINGS:
                out.add(zh)
    return out


def tr_sources():
    """所有 tr("中文") 源串（走 Qt .ts/.qm 翻译链）。"""
    out = set()
    for rel, p in iter_sources():
        with open(p, encoding="utf-8", errors="replace") as f:
            s = f.read()
        for m in re.finditer(r'\btr\("((?:[^"\\]|\\.)*)"\)', s):
            zh = unescape(m.group(1))
            if CJK.search(zh):
                out.add(zh)
    return out


# ---------------------------------------------------------------------------
# 生成物
# ---------------------------------------------------------------------------
def cpp_raw_blob(lines):
    return "\n".join(lines)


def load_sources():
    """读取持久化的中文源串清单（包装后无法再扫描出来，必须保存）。"""
    if not os.path.exists(SOURCES_JSON):
        return []
    import json
    with open(SOURCES_JSON, encoding="utf-8") as f:
        data = json.load(f)
    return [x for x in data if isinstance(x, str) and CJK.search(x)]


def save_sources(ui):
    import json
    with open(SOURCES_JSON, "w", encoding="utf-8") as f:
        json.dump(sorted(ui), f, ensure_ascii=False, indent=1)


WRAPPED_RE = re.compile(
    r'(?:SketchCAD::)?I18n::(?:t|tUtf8)\s*\(\s*'
    r'(?:(?:QStringLiteral|QString::fromUtf8|tr)\s*\(\s*)?'
    r'"((?:[^"\\]|\\.)*)"')


def wrapped_sources():
    """从已包装的源码反查中文源串。

    --apply 之后 scan() 会跳过所有 I18n::t(...) 字面量，若只依赖扫描结果，
    第二次运行生成器词典会从 3000+ 条缩到几百条。这里直接把包装器里的
    源串取回来，使生成器完全幂等、且不依赖持久化 JSON 的完整性。
    """
    out = set()
    for rel, p in iter_sources():
        if rel in SKIP_FILES:
            continue
        with open(p, encoding="utf-8", errors="replace") as f:
            s = f.read()
        for m in WRAPPED_RE.finditer(s):
            zh = unescape(m.group(1))
            if CJK.search(zh) and zh not in EXCLUDE_STRINGS:
                out.add(zh)
    return out


def extra_sources():
    if not os.path.exists(SOURCES_EXTRA):
        return set()
    out = set()
    with open(SOURCES_EXTRA, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.lstrip().startswith("#"):
                continue
            zh = unescape(line)
            if CJK.search(zh):
                out.add(zh)
    return out


def all_sources():
    """当前代码库里全部用户可见中文源串（权威集合）。"""
    ui = set(wrapped_sources())
    ui |= sink_strings()
    ui |= tr_sources()
    ui |= extra_sources()
    ui |= set(load_sources())
    for _rel, _p, _s, hits in scan():
        for _a, _b, zh, _oc in hits:
            ui.add(zh)
    return ui


def gen_dict_cpp(exact, terms):
    # 每行用 TAB 分隔键值（TAB 在 escape() 后不会出现在数据里）；
    # 换行/反斜杠/TAB 统一做 C 风格转义，避免多行文案把数据行切断。
    exact_lines = ["%s\t%s" % (escape(k), escape(v))
                   for k, v in sorted(exact.items())]
    term_lines = ["%s\t%s" % (escape(k), escape(v))
                  for k, v in sorted(terms.items())]
    ex = cpp_raw_blob(exact_lines)
    tm = cpp_raw_blob(term_lines)
    # C++ raw string 定界符不能含 ')'；形如 R"TAG( ... )TAG"
    assert ")I18N_DICT_END\"" not in ex and ")I18N_DICT_END\"" not in tm
    return '''// =============================================================================
// ⚠️ 自动生成文件 —— 请勿手工编辑。
// 生成器：tools/i18n/build_i18n.py（--apply）
// 数据  ：tools/i18n/en_phrases.txt + tools/i18n/en_glossary.txt
//
// 用单个 raw string 数据块 + 惰性解析，而不是数千条 QStringLiteral 聚合初始化：
// MinGW GCC 13.1 (cc1plus) 处理巨型聚合初始化会静默崩溃（EXIT=1 零诊断）。
// =============================================================================
#include "I18nDict.h"

#include <QStringList>

namespace SketchCAD {
namespace I18n {
namespace Runtime {

namespace {

const char* const kExactData = R"I18N_DICT_END(
%s
)I18N_DICT_END";

const char* const kTermData = R"I18N_DICT_END(
%s
)I18N_DICT_END";

// 反转义 escape() 生成的 C 风格序列（\\\\ \\" \\n \\r \\t \\0）。
QString unescape(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c != QLatin1Char('\\\\') || i + 1 >= s.size()) {
            out.append(c);
            continue;
        }
        const QChar n = s.at(++i);
        switch (n.unicode()) {
        case 'n':  out.append(QLatin1Char('\\n')); break;
        case 't':  out.append(QLatin1Char('\\t')); break;
        case 'r':  out.append(QLatin1Char('\\r')); break;
        case '0':  out.append(QChar(0)); break;
        case '\\\\': out.append(QLatin1Char('\\\\')); break;
        case '"': out.append(QLatin1Char('"')); break;
        default:   out.append(n); break;
        }
    }
    return out;
}

QHash<QString, QString> parse(const char* data, int* maxKeyLen) {
    QHash<QString, QString> map;
    const QString blob = QString::fromUtf8(data);
    const QStringList rows = blob.split(QLatin1Char('\\n'));
    int maxLen = 1;
    for (const QString& row : rows) {
        // 数据行格式：escape(key) TAB escape(value)；键里可能含 '='，
        // 值里可能含 '='，所以只能按第一个 TAB 切分。
        const int tab = row.indexOf(QLatin1Char('\\t'));
        if (tab <= 0) continue;
        const QString key = unescape(row.left(tab));
        const QString val = unescape(row.mid(tab + 1));
        if (key.isEmpty()) continue;
        // 术语表里允许空译文（语法助词「的/了/把」等）：scrubByTerms 命中后
        // 直接丢弃该汉字，这正是动态拼接文案去掉中文残留所需要的。
        map.insert(key, val);
        if (key.size() > maxLen) maxLen = key.size();
    }
    if (maxKeyLen) *maxKeyLen = maxLen;
    return map;
}

} // namespace

const QHash<QString, QString>& exactMap() {
    static const QHash<QString, QString> map = parse(kExactData, nullptr);
    return map;
}

const QHash<QString, QString>& termMap() {
    static int maxLen = 1;
    static const QHash<QString, QString> map = parse(kTermData, &maxLen);
    return map;
}

int maxTermLength() {
    termMap();
    // parse() 已把最长键长写入静态 maxLen（见上方 static 局部）
    // 这里独立重算一次，避免暴露内部状态；成本 O(词条数) 且仅首次调用。
    static int cached = -1;
    if (cached < 0) {
        cached = 1;
        for (auto it = termMap().constBegin(); it != termMap().constEnd(); ++it)
            if (it.key().size() > cached) cached = it.key().size();
    }
    return cached;
}

} // namespace Runtime
} // namespace I18n
} // namespace SketchCAD
''' % (ex, tm)


INCLUDE_LINE = '#include "core/I18n.h"'


def find_tr_calls(s):
    """找出未包装的 tr(...) 调用（支持多字面量拼接/嵌套括号），返回 (start,end,text)。"""
    mask = comment_mask(s)
    out, i, n = [], 0, len(s)
    while i < n:
        if mask[i]:
            i += 1
            continue
        c = s[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n:
                if s[j] == "\\":
                    j += 2
                    continue
                if s[j] == q:
                    break
                j += 1
            i = j + 1
            continue
        if s.startswith("tr(", i) and not (i and (s[i - 1].isalnum() or s[i - 1] == "_")):
            if not s[max(0, i - 24):i].rstrip().endswith("I18n::t("):
                k, depth = i + 3, 1        # 跳过 "tr(" 三个字符，k 指向括号内首字符
                while k < n and depth > 0:
                    if mask[k]:
                        k += 1
                        continue
                    if s[k] == '"' or s[k] == "'":
                        q = s[k]
                        j = k + 1
                        while j < n:
                            if s[j] == "\\":
                                j += 2
                                continue
                            if s[j] == q:
                                break
                            j += 1
                        k = j + 1
                        continue
                    if s[k] == "(":
                        depth += 1
                    elif s[k] == ")":
                        depth -= 1
                    k += 1
                if depth == 0:
                    body = s[i + 3:k - 1]
                    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
                    if any(CJK.search(unescape(x)) for x in lits):
                        out.append((i, k, s[i:k]))
                    i = k
                    continue
        i += 1
    return out


def apply_edits(s, edits):
    """应用互不重叠的替换，并按需补 include。"""
    if not edits:
        return s, 0
    edits.sort(key=lambda e: (e[0], -(e[1])))
    merged = []
    for a, b, repl in edits:
        if merged and a < merged[-1][1]:
            continue
        merged.append((a, b, repl))
    out = s
    for a, b, repl in reversed(merged):
        out = out[:a] + repl + out[b:]
    if INCLUDE_LINE not in out:
        m = re.search(r'^#include\s+[<"]', out, re.M)
        if m:
            out = out[:m.start()] + INCLUDE_LINE + "\n" + out[m.start():]
        else:
            out = INCLUDE_LINE + "\n" + out
    return out, len(merged)


def wrap_sources(scanned):
    changed = []
    touched = set()
    for rel, p, s, hits in scanned:
        edits = []
        for a, b, zh, _oc in hits:
            # 整段（含相邻字面量拼接）一次性译成 QString，避免破坏 C++ 拼接语义
            repl = ('SketchCAD::I18n::t(QString::fromUtf8("'
                    + escape(zh) + '"))')
            edits.append((a, b, repl))
        for a, b, text in find_tr_calls(s):
            edits.append((a, b, "SketchCAD::I18n::t(" + text + ")"))
        out, cnt = apply_edits(s, edits)
        if cnt:
            with open(p, "w", encoding="utf-8", newline="") as f:
                f.write(out)
            touched.add(rel)
            changed.append((rel, cnt))

    # 第二遍：其余文件里零散的 tr("中文") 也必须叠运行时词典兜底
    for rel, p in iter_sources():
        if rel in touched or rel in SKIP_FILES or rel in SINK_FILES:
            continue
        with open(p, encoding="utf-8", errors="replace") as f:
            s = f.read()
        edits = [(a, b, "SketchCAD::I18n::t(" + text + ")")
                 for a, b, text in find_tr_calls(s)]
        out, cnt = apply_edits(s, edits)
        if cnt:
            with open(p, "w", encoding="utf-8", newline="") as f:
                f.write(out)
            changed.append((rel, cnt))
    return changed


def xml_escape(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;")
             .replace(">", "&gt;").replace('"', "&quot;"))


def extend_ts(tr_map):
    """只补齐既有 .ts 消息里缺失/未完成/空的译文，不改动已有译文与上下文结构。
    tr() 全量兜底由源码包装「I18n::t(tr(...))」+ 运行时词典承担，因此这里
    不新增 context，避免影响 IL3~IL10 既有翻译链自测。"""
    import xml.etree.ElementTree as ET
    tree = ET.parse(TS)
    root = tree.getroot()
    filled = 0
    total = 0
    for msg in root.findall(".//message"):
        total += 1
        src = msg.findtext("source") or ""
        node = msg.find("translation")
        if node is None:
            continue
        if (node.text or "").strip() and node.attrib.get("type") != "unfinished":
            continue
        if src in tr_map:
            node.text = tr_map[src]
            node.attrib.pop("type", None)
            filled += 1
    if filled:
        with open(TS, "wb") as f:
            tree.write(f, encoding="utf-8", xml_declaration=True)
    return filled, total


# ---------------------------------------------------------------------------
# 校验
# ---------------------------------------------------------------------------
def patch_sinks():
    """统一文本出口 + CMake 注册（幂等精确替换；--apply 时一次生成全部代码改动）。"""
    patches = [
        # 1) 命令行 / 状态 / 错误反馈总出口兜底
        ("src/core/SketchModel.cpp",
         "void SketchModel::log(const QString& m, const QString& t) { emit logAppended(m, t); }",
         "// 统一反馈出口兜底：绝大多数命令行反馈/错误/状态已在字面量处包装过\n"
         "// I18n::t()，这里再清洗一次，覆盖「运行时拼接 + 中文术语实参」等动态组合。\n"
         "// 中文模式恒等返回；已是英文的消息（无汉字）直接透传，不会二次翻译。\n"
         "void SketchModel::log(const QString& m, const QString& t) {\n"
         "    emit logAppended(SketchCAD::I18n::t(m), t);\n"
         "}"),
        # 2) 命令目录元数据（帮助 / 命令面板 / AI prompt 的中文来源）
        ("src/core/CommandCatalog.cpp",
         '#include "CommandCatalog.h"\n#include <QHash>',
         '#include "CommandCatalog.h"\n#include "core/I18n.h"\n#include <QHash>'),
        ("src/core/CommandCatalog.cpp",
         "    static const QVector<CommandInfo> table = [] {\n        QVector<CommandInfo> t;",
         "    // 语言切换后重建（中文源串保持不变，只在组装时做显示层翻译）。\n"
         "    static QVector<CommandInfo> table;\n"
         "    static QString tableLang;\n"
         "    const QString lang = SketchCAD::I18n::activeLanguage();\n"
         "    if (!table.isEmpty() && tableLang == lang) return table;\n"
         "    table = [] {\n        QVector<CommandInfo> t;"),
        ("src/core/CommandCatalog.cpp",
         "            ci.name = QString::fromUtf8(name);\n"
         "            ci.zhName = QString::fromUtf8(zh);\n"
         "            ci.category = QString::fromUtf8(cat);\n"
         "            ci.summary = QString::fromUtf8(sum);\n"
         "            ci.usage = QString::fromUtf8(usage);\n"
         "            ci.params = QString::fromUtf8(params);\n"
         "            ci.example = QString::fromUtf8(ex);",
         "            ci.name = QString::fromUtf8(name);       // 命令 id 恒 ASCII，绝不翻译\n"
         "            ci.zhName = SketchCAD::I18n::tUtf8(zh);\n"
         "            ci.category = SketchCAD::I18n::tUtf8(cat);\n"
         "            ci.summary = SketchCAD::I18n::tUtf8(sum);\n"
         "            ci.usage = SketchCAD::I18n::tUtf8(usage);\n"
         "            ci.params = SketchCAD::I18n::tUtf8(params);\n"
         "            ci.example = SketchCAD::I18n::tUtf8(ex);"),
        ("src/core/CommandCatalog.cpp",
         "        return t;\n    }();\n    return table;\n}",
         "        return t;\n    }();\n    tableLang = lang;\n    return table;\n}"),
        # 3) 工具注册表显示名/参数提示（惰性语言缓存，切换即时生效）
        ("src/core/ToolRegistry.cpp",
         '#include "core/ToolRegistry.h"\n\nnamespace SketchCAD {',
         '#include "core/ToolRegistry.h"\n#include "core/I18n.h"\n\nnamespace SketchCAD {'),
        ("src/core/ToolRegistry.cpp",
         "const ToolSpec* ToolRegistry::find(const QString& id) const {\n"
         "    auto it = tools_.constFind(id);\n"
         "    return it != tools_.constEnd() ? &it.value() : nullptr;\n}",
         "const ToolSpec* ToolRegistry::find(const QString& id) const {\n"
         "    // 显示字段按当前界面语言惰性翻译；中文模式原样返回，英文模式查运行时词典。\n"
         "    // 分类/参数模式等枚举字段随拷贝保留，所有分类查询继续走这里零行为变化。\n"
         "    const QString lang = SketchCAD::I18n::activeLanguage();\n"
         "    if (localizedLang_ != lang || localizedCount_ != tools_.size()) {\n"
         "        localized_.clear();\n"
         "        localized_.reserve(tools_.size());\n"
         "        for (auto i = tools_.constBegin(); i != tools_.constEnd(); ++i) {\n"
         "            ToolSpec s = i.value();\n"
         "            s.displayName = SketchCAD::I18n::t(s.displayName);\n"
         "            s.paramHint   = SketchCAD::I18n::t(s.paramHint);\n"
         "            localized_.insert(i.key(), std::move(s));\n"
         "        }\n"
         "        localizedLang_  = lang;\n"
         "        localizedCount_ = tools_.size();\n"
         "    }\n"
         "    auto it = localized_.constFind(id);\n"
         "    return it != localized_.constEnd() ? &it.value() : nullptr;\n}"),
        ("src/core/ToolRegistry.h",
         "    QHash<QString, ToolSpec> tools_;\n    QStringList lineSubIds_;",
         "    QHash<QString, ToolSpec> tools_;\n"
         "    // 英文模式的显示名/提示惰性缓存：按 activeLanguage() 失效重建。\n"
         "    mutable QHash<QString, ToolSpec> localized_;\n"
         "    mutable QString localizedLang_;\n"
         "    mutable int localizedCount_ = 0;\n"
         "    QStringList lineSubIds_;"),
        # 4) 生成词典加入两个目标的公共源列表
        ("CMakeLists.txt",
         "    src/core/I18n.cpp          # 界面语言单一入口（语言解析 + QTranslator 装载，自测直测）",
         "    src/core/I18n.cpp          # 界面语言单一入口（语言解析 + QTranslator 装载，自测直测）\n"
         "    src/core/I18nDict.cpp      # 生成式中文 UI→英文词典（tools/i18n/build_i18n.py 产出，勿手改）"),
    ]
    applied = []
    for rel, old, new in patches:
        p = os.path.join(ROOT, rel)
        with open(p, encoding="utf-8") as f:
            s = f.read()
        if new in s:
            continue                      # 幂等：已打过补丁
        if old not in s:
            raise RuntimeError("patch_sinks: 锚点未命中 %s :: %r" % (rel, old[:60]))
        s = s.replace(old, new, 1)
        with open(p, "w", encoding="utf-8", newline="") as f:
            f.write(s)
        applied.append(rel)
    return applied


def verify(ui_strings, exact, report_lines):
    problems = []
    for zh in sorted(ui_strings):
        en = exact.get(zh)
        if en is None:
            problems.append("NO-TRANSLATION %r" % zh)
            continue
        if CJK.search(en):
            problems.append("CJK-IN-ENGLISH %r -> %r" % (zh, en))
        if CJK_PUNCT.search(en):
            problems.append("CJK-PUNCT-IN-ENGLISH %r -> %r" % (zh, en))
        if len(PLACEHOLDER.findall(zh)) != len(PLACEHOLDER.findall(en)):
            problems.append("PLACEHOLDER-MISMATCH %r -> %r" % (zh, en))
        if not en.strip():
            problems.append("EMPTY-TRANSLATION %r" % zh)
    report_lines.append("校验：源串 %d / 译文 %d / 问题 %d"
                        % (len(ui_strings), len(exact), len(problems)))
    for p in problems[:80]:
        report_lines.append("  " + p)
    if len(problems) > 80:
        report_lines.append("  ... 其余 %d 条" % (len(problems) - 80))
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="写回源码/词典/.ts")
    ap.add_argument("--check", action="store_true", help="只校验，不写文件")
    ap.add_argument("--rebuild-sources", action="store_true",
                    help="丢弃持久化清单，直接从当前源码重建 en_sources.json")
    args = ap.parse_args()

    if args.rebuild_sources:
        authoritative = set(wrapped_sources()) | sink_strings() | tr_sources()
        for _rel, _p, _s, hits in scan():
            for _a, _b, zh, _oc in hits:
                authoritative.add(zh)
        save_sources(authoritative)
        print("rebuilt sources: %d" % len(authoritative))
        if not args.apply and not args.check:
            return 0

    scanned = scan()
    ui = all_sources()

    # 词典也要覆盖术语表键本身（供 termScrub 对动态串做清洗）
    exact = {zh: translate(zh) for zh in ui}
    # 术语清洗表：所有纯中文术语 + 词典里的短标签（≤12 字且无占位符）
    terms = dict(TERM_MAP)
    for zh in ui:
        if PURE_CJK.match(zh) and len(zh) <= 16 and "%" not in zh:
            terms.setdefault(zh, exact[zh])

    report = []
    report.append("SketchCAD 英文界面翻译覆盖报告")
    report.append("=" * 72)
    report.append("用户可见中文源串（已包装/元数据表/tr 源串去重）: %d" % len(ui))
    report.append("术语表条目: %d（最长键 %d 字）" % (len(terms), MAX_TERM_LEN))
    report.append("人工校准整句: %d" % len(PHRASE_MAP))
    byfile = collections.Counter()
    for rel, _p, _s, hits in scanned:
        byfile[rel] += len(hits)
    report.append("待包装字面量: %d 处，分布于 %d 个文件" %
                  (sum(byfile.values()), len(byfile)))
    for rel, c in byfile.most_common(25):
        report.append("  %5d  %s" % (c, rel))
    problems = verify(ui, exact, report)

    with open(REPORT, "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report[:12]))
    if problems:
        print("... 详见 %s" % REPORT)
        print("FAILED: %d 条译文不合格" % len(problems))
        return 1
    print("OK: 全部 %d 条译文无中文残留且占位符一致" % len(ui))

    if args.apply:
        save_sources(ui)
        with open(DICT_CPP, "w", encoding="utf-8", newline="\n") as f:
            f.write(gen_dict_cpp(exact, terms))
        changed = wrap_sources(scanned)
        sinks = patch_sinks()
        filled, total = extend_ts({zh: exact[zh] for zh in tr_sources()})
        print("生成 %s" % DICT_CPP)
        print("包装 %d 个文件 / %d 处字面量" %
              (len(changed), sum(c for _, c in changed)))
        print("统一出口补丁：%d 个文件" % len(sinks))
        print("补齐 .ts 缺失译文：%d 条（既有消息共 %d 条）" % (filled, total))
        with open(REPORT, "a", encoding="utf-8") as f:
            f.write("\n--apply：包装 %d 文件 / %d 处；.ts 补齐 %d 条\n" %
                    (len(changed), sum(c for _, c in changed), filled))
            for rel, c in changed:
                f.write("  %5d  %s\n" % (c, rel))
    return 0


if __name__ == "__main__":
    sys.exit(main())
