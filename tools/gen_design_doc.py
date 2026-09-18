# -*- coding: utf-8 -*-
"""生成《航空CAD-草图绘制模块-C++实现设计思考文档》"""
import os
from docx import Document
from docx.shared import Pt, RGBColor, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

OUT = r"D:\01-CAD\02-avticationCAD\docs\航空CAD-草图绘制模块-C++实现设计思考文档.docx"

CN = "微软雅黑"
MONO = "Consolas"
ACCENT = RGBColor(0x1F, 0x4E, 0x79)
GREY = RGBColor(0x59, 0x59, 0x59)


def set_cn(run, font=CN):
    run.font.name = font
    run._element.rPr.rFonts.set(qn("w:eastAsia"), font)


def shade(cell, color):
    tcPr = cell._tc.get_or_add_tcPr()
    el = OxmlElement("w:shd")
    el.set(qn("w:val"), "clear")
    el.set(qn("w:fill"), color)
    tcPr.append(el)


def setup_styles(doc):
    st = doc.styles["Normal"]
    st.font.name = CN
    st.font.size = Pt(10.5)
    st.element.rPr.rFonts.set(qn("w:eastAsia"), CN)
    st.paragraph_format.line_spacing = 1.45
    st.paragraph_format.space_after = Pt(4)
    for name, size, color in (("Heading 1", 17, ACCENT),
                              ("Heading 2", 14, ACCENT),
                              ("Heading 3", 12, ACCENT)):
        s = doc.styles[name]
        s.font.name = CN
        s.font.size = Pt(size)
        s.font.bold = True
        s.font.color.rgb = color
        s.element.rPr.rFonts.set(qn("w:eastAsia"), CN)
        s.paragraph_format.space_before = Pt(14)
        s.paragraph_format.space_after = Pt(7)


def h(doc, text, level=1):
    p = doc.add_heading(text, level=level)
    for r in p.runs:
        set_cn(r)
    return p


def para(doc, text, bold=False, italic=False, color=None, size=10.5, align=None):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.bold = bold
    r.italic = italic
    r.font.size = Pt(size)
    if color:
        r.font.color.rgb = color
    set_cn(r)
    if align:
        p.alignment = align
    return p


def bullet(doc, text, level=0):
    p = doc.add_paragraph(style="List Bullet")
    p.paragraph_format.left_indent = Cm(0.75 + level * 0.75)
    parts = text.split("**")
    for i, seg in enumerate(parts):
        if not seg:
            continue
        r = p.add_run(seg)
        r.bold = (i % 2 == 1)
        r.font.size = Pt(10.5)
        set_cn(r)
    return p


def code(doc, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.6)
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(8)
    p.paragraph_format.line_spacing = 1.15
    r = p.add_run(text)
    r.font.name = MONO
    r.font.size = Pt(9)
    r.font.color.rgb = RGBColor(0x1A, 0x1A, 0x1A)
    r._element.rPr.rFonts.set(qn("w:eastAsia"), MONO)
    pPr = p._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:fill"), "F4F6F8")
    pPr.append(shd)
    return p


def table(doc, header, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(header))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, txt in enumerate(header):
        c = t.rows[0].cells[i]
        c.text = ""
        r = c.paragraphs[0].add_run(txt)
        r.bold = True
        r.font.size = Pt(9.5)
        r.font.color.rgb = RGBColor(0xFF, 0xFF, 0xFF)
        set_cn(r)
        c.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
        shade(c, "1F4E79")
    for row in rows:
        cells = t.add_row().cells
        for i, txt in enumerate(row):
            cells[i].text = ""
            p = cells[i].paragraphs[0]
            p.paragraph_format.line_spacing = 1.25
            r = p.add_run(str(txt))
            r.font.size = Pt(9.5)
            set_cn(r)
    if widths:
        for row in t.rows:
            for i, w in enumerate(widths):
                row.cells[i].width = Cm(w)
    doc.add_paragraph()
    return t


def caption(doc, text):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.font.size = Pt(9)
    r.italic = True
    r.font.color.rgb = GREY
    set_cn(r)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    return p


def build():
    doc = Document()
    setup_styles(doc)
    sec = doc.sections[0]
    sec.left_margin = sec.right_margin = Cm(2.4)
    sec.top_margin = sec.bottom_margin = Cm(2.2)

    # ---------------- 封面 ----------------
    doc.add_paragraph()
    doc.add_paragraph()
    p = doc.add_paragraph()
    r = p.add_run("航空CAD 草图绘制模块")
    r.font.size = Pt(28)
    r.bold = True
    r.font.color.rgb = ACCENT
    set_cn(r)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER

    p = doc.add_paragraph()
    r = p.add_run("C++17 实现 · 设计思考文档")
    r.font.size = Pt(16)
    r.font.color.rgb = GREY
    set_cn(r)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER

    doc.add_paragraph()
    para(doc,
         "本文记录该模块从需求解读到落地实现全过程中的关键判断：为什么这样分层、"
         "每个岔路口有哪些候选方案、为什么选了其中一个、哪些能力选择了不做以及理由。"
         "它不是接口手册，而是设计决策的账本。",
         italic=True, color=GREY, align=WD_ALIGN_PARAGRAPH.CENTER)

    doc.add_paragraph()
    table(doc,
          ["项目", "内容"],
          [["模块名称", "航空CAD 系统 — 草图绘制模块"],
           ["实现语言", "C++17（GNU C++17 方言）"],
           ["GUI 框架", "Qt 6.10.1（Widgets）"],
           ["编译工具链", "MinGW-w64 GCC 13.1.0 + CMake 3.30 + Ninja 1.12"],
           ["代码规模", "26 个源文件，约 2,300 行（含 200 行自测）"],
           ["自测结果", "57 项断言全部通过，编译零告警"],
           ["需求依据", "《航空CAD-草图绘制-详细功能规格与设计说明书》"],
           ["文档版本", "v1.0"]],
          widths=[4.0, 11.5])

    doc.add_page_break()

    # ---------------- 1 ----------------
    h(doc, "1  设计出发点：这是一个什么性质的问题", 1)

    para(doc,
         "拿到需求后，第一件事不是画类图，而是判断这个模块的难点究竟在哪里。"
         "界面布局那一段描述得很细——五个面板、菜单栏、工具条、命令行、状态栏——"
         "容易让人误以为这是一个「界面搭建」任务。但真正决定成败的是需求里两句不起眼的话：")

    bullet(doc, "「所有绘图操作均支持**鼠标交互与键盘命令两种方式**」")
    bullet(doc, "「图形区与各面板间实现**数据联动**」")

    para(doc,
         "这两句话合起来意味着：同一个语义动作（比如「画一条直线」）有至少三条触发路径"
         "——工具条按钮、图形区连续点击、命令行敲 line 0,0 100,0；"
         "而这个动作产生的后果要同时反映到图形区、BOM 树、属性面板、执行信息面板、状态栏五个地方。"
         "如果按「界面搭建」的思路做，就会写出 3 × 5 = 15 处两两耦合的调用，"
         "每加一个工具、每加一个面板，工作量都是乘法增长的。")

    para(doc, "所以核心问题被重述为：", bold=True)
    para(doc,
         "如何让「操作的输入方式」与「状态的展示方式」彼此不知道对方存在？")

    para(doc,
         "这个重述直接决定了后面所有的架构选择。它把一个界面题变成了一个解耦题。")

    # ---------------- 2 ----------------
    h(doc, "2  架构决策：单一数据源 + 信号驱动", 1)

    h(doc, "2.1  三个候选方案的取舍", 2)

    table(doc,
          ["方案", "做法", "为什么没选 / 为什么选"],
          [["面板直连",
            "工具条直接调用图形区方法，图形区直接刷新 BOM 树",
            "落选。N 个输入 × M 个输出的耦合矩阵，改任何一处都要动多个文件；"
            "命令行这条通路会变成对鼠标通路的整段复制"],
           ["MVC / MVP",
            "为每个面板配一个 Presenter",
            "落选。面板之间大量共享同一份图元数据，拆成多个 Presenter 后"
            "反而要在 Presenter 之间再同步一次，把问题推后而非解决"],
           ["单一模型 + 信号广播",
            "所有状态收敛到 SketchModel；改动只经它，它发信号，面板各自订阅",
            "**采用**。输入方 只依赖模型，展示方 只依赖信号，"
            "两边互不知情。新增一个工具或一个面板都是加法而不是乘法"]],
          widths=[2.8, 5.0, 7.7])

    h(doc, "2.2  最终的数据流", 2)

    code(doc,
         "┌───────── 输入层（三条通路，彼此对等）─────────┐\n"
         "  工具条 / 菜单 / 快捷键     图形区鼠标拾取     命令行文本\n"
         "        │                        │                 │\n"
         "        └────────────┬───────────┴─────────────────┘\n"
         "                     ▼\n"
         "        ┌────────────────────────────┐\n"
         "        │   SketchModel（唯一真相源）  │\n"
         "        │   图元表 / 选中 / 视图状态    │\n"
         "        │   撤销栈 / 剪贴板            │\n"
         "        └────────────┬───────────────┘\n"
         "                     │ Qt signals（单向广播）\n"
         "   ┌─────────┬───────┼────────┬──────────┬─────────┐\n"
         "   ▼         ▼       ▼        ▼          ▼         ▼\n"
         " 图形区    BOM树   属性面板  执行信息   状态栏   工具条高亮")

    caption(doc, "图 2-1  单向数据流：输入只写模型，展示只读信号")

    para(doc,
         "这里有一条被严格执行的纪律：**任何面板都不允许直接修改另一个面板**。"
         "属性面板改了坐标，它调 model_->updateElement()，然后由模型广播 modelChanged，"
         "图形区自己重绘。属性面板从头到尾不知道图形区的存在。")

    h(doc, "2.3  信号清单与它们各自回答的问题", 2)

    table(doc,
          ["信号", "回答的问题", "订阅方"],
          [["modelChanged", "图元集合变了吗", "图形区、BOM 树、属性面板、状态栏"],
           ["selectionChanged", "现在选中的是谁", "图形区、BOM 树、属性面板"],
           ["logAppended", "刚才发生了什么", "执行信息面板"],
           ["statusCommandChanged", "当前处于什么命令下", "状态栏"],
           ["activeToolChanged", "激活的工具是哪个", "工具条高亮、图形区（中止绘制）"],
           ["planeChanged", "工作平面切到哪了", "图形区、状态栏"],
           ["gridToggled", "栅格开还是关", "图形区、视图菜单勾选态"],
           ["viewChanged", "缩放/平移变了吗", "图形区、状态栏"],
           ["cursorMoved", "鼠标现在在哪", "状态栏坐标显示"]],
          widths=[4.2, 6.3, 5.0])

    para(doc,
         "刻意拆成 9 个细粒度信号而不是一个 somethingChanged，"
         "是因为重绘代价差异巨大：鼠标移动每秒触发几十次 cursorMoved，"
         "只需要更新状态栏两个数字；而 modelChanged 要重建整个场景。"
         "如果合并成一个信号，鼠标一动就重建场景，交互会立刻卡死。",)

    doc.add_page_break()

    # ---------------- 3 ----------------
    h(doc, "3  关键取舍逐条记录", 1)

    h(doc, "3.1  为什么撤销用「快照」而不是「命令模式」", 2)

    para(doc,
         "教科书答案是命令模式：每个操作实现 execute/undo 一对方法。它内存效率高，"
         "是大型 CAD 的标准做法。但这里选了整体快照——每次修改前把 std::vector<Element> "
         "整个复制一份压栈。")

    para(doc, "理由是三条现实约束：", bold=True)
    bullet(doc,
           "**正确性成本不对称**。命令模式要求每个操作都写出严格互逆的一对函数。"
           "本模块有 8 类图元、6 种编辑操作，任何一个 undo 写错都会造成数据静默损坏，"
           "而这类 bug 极难复现。快照法在结构上不可能写错。")
    bullet(doc,
           "**内存代价可算清**。单个 Element 约 200 字节，草图典型规模数百个图元，"
           "一次快照约 100 KB，栈深上限 200 步 → 峰值约 20 MB。这在桌面端完全可接受。")
    bullet(doc,
           "**保留了升级路径**。Memento 结构体带 label 字段，将来接入几何约束求解器、"
           "图元数量上万时，可以把它换成增量 diff，而调用方（model_->undo()）一行都不用改。")

    para(doc,
         "这是一次明确的「用内存换正确性」的交易，并且交易边界是清楚的。",
         italic=True, color=GREY)

    h(doc, "3.2  矩形为什么只存两个对角点", 2)

    para(doc,
         "矩形可以存 4 个角点（渲染直接、导出方便），也可以存 2 个对角点（无冗余）。"
         "选了后者，因为 4 点表示会引入一个不变量：这 4 个点必须始终构成矩形。"
         "一旦属性面板允许用户单独拖动某个角点，这个不变量就会被破坏，"
         "得到一个「类型是矩形但形状是任意四边形」的非法对象。")

    para(doc,
         "2 点表示让非法状态在结构上无法表达。代价是渲染和 DXF 导出时要展开成 4 条边，"
         "但这是一次性的、局部的、可测试的转换。这条原则在自测里被固化为断言："
         "「矩形按两对角点存储」。")

    h(doc, "3.3  修剪与延伸：从「待接入」到真正算出来", 2)

    para(doc,
         "初版把 trim / extend / constraint / dim 四个工具统一处理为打一条"
         "「功能待接入（几何内核后续实现）」的日志。复盘时判断这个偷懒不成立：")

    bullet(doc,
           "标注的本质是两点间距离，就是一个 sqrt，没有任何需要「几何内核」的地方；")
    bullet(doc,
           "直线的修剪与延伸只需要两条直线求交，是初等解析几何，"
           "把它归入「内核」属于对难度的错误估计。")

    para(doc, "于是重新实现。求交采用参数式而非斜截式，避免竖直线的除零：")

    code(doc,
         "den = (x1-x2)(y3-y4) - (y1-y2)(x3-x4);\n"
         "if (fabs(den) < 1e-9) return false;          // 平行或共线\n"
         "t   = ((x1-x3)(y3-y4) - (y1-y3)(x3-x4)) / den;\n"
         "P   = (x1 + t(x2-x1),  y1 + t(y2-y1));")

    para(doc,
         "实现过程中有一个有意思的发现：**修剪与延伸的计算完全相同**"
         "——都是「把离点击处最近的那个端点移到交点」。"
         "两者的真正区别不在算法，而在语义前提：修剪要求交点落在目标线段内部，"
         "延伸要求落在外部。于是用投影参数 s 做了一次判定，"
         "并在用户选错工具时给出可执行的提示（「交点不在线段内，可改用延伸」），"
         "而不是默默算出一个反直觉的结果。")

    para(doc,
         "只有几何约束求解器保留为规划项，因为它需要牛顿迭代与欠约束/过约束诊断，"
         "是真正的独立子系统。当前版本如实落库约束关系（refA / refB）并在日志中说明，"
         "不假装已经求解。",)

    h(doc, "3.4  命令行：Tab 补全为什么补「公共前缀」", 2)

    para(doc,
         "命令行是键盘通路的入口，它的体验决定了「支持键盘命令」这条需求是真落地还是走形式。"
         "三个细节：")

    table(doc,
          ["能力", "实现要点", "背后的考虑"],
          [["历史回溯",
            "↑/↓ 遍历，指针越界时清空输入框",
            "与终端行为一致，用户不需要重新学习"],
           ["Tab 补全",
            "多候选时补到**最长公共前缀**并列出全部候选",
            "直接补第一个候选会造成误输入；"
            "只列候选不补又浪费按键。公共前缀是两者的平衡点"],
           ["点格式宽容",
            "x,y / x y / x,y,z 都接受",
            "手输坐标时逗号和空格的混用是高频行为，"
            "在解析层用一个正则吸收掉，好过让用户记规则"]],
          widths=[3.0, 5.5, 7.0])

    para(doc,
         "补全实现里有一个容易忽略的正确性点：前缀 li 会同时匹配 line 和 list。"
         "自测最初写成「li 唯一补全为 line」并因此报错——"
         "这是测试期望写错了，不是代码错了。修正测试的同时补了 sp → spline 的唯一匹配用例，"
         "把两种情况都覆盖住。")

    h(doc, "3.5  一个真实缺陷：拖拽创建全变成直线", 2)

    para(doc,
         "自测跑通之后逐行走查交互链路，发现了一个静默失效：")

    code(doc,
         "// 元素面板放进 MIME 的是英文 id\n"
         "it->setData(Qt::UserRole, \"circle\");\n\n"
         "// 图形区却按中文名匹配\n"
         "if      (type == \"直线\") { ... }\n"
         "else if (type == \"圆\")   { ... }\n"
         "else                      { e.type = ElementType::Line; }  // ← 全部落到这里")

    para(doc,
         "后果是：从元素面板拖任何图元到画布，都会创建一条名叫 \"circle\" 的直线。"
         "程序不崩溃、不报错、日志还显示「拖拽创建成功」——这类缺陷比崩溃危险得多。")

    para(doc, "修复不止是把中文换成英文，而是同时消除了产生这类问题的土壤：", bold=True)
    bullet(doc, "**确立唯一标识**：工具条、元素面板、命令行统一使用英文 id（line/circle/...），"
                "中文只作为显示名；")
    bullet(doc, "**保留别名映射**：万一上游传入显示名也能正确解析，不再静默退化；")
    bullet(doc, "**去掉兜底分支**：未知类型改为记录警告并拒绝创建。"
                "宁可不做事，也不要做错事还装作成功。")

    para(doc,
         "最后一条是这次修复里最有价值的部分：一个 else 兜底看似「健壮」，"
         "实际上把类型错误转化成了数据错误。",
         italic=True, color=GREY)

    doc.add_page_break()

    # ---------------- 4 ----------------
    h(doc, "4  界面布局的实现思路", 1)

    h(doc, "4.1  用 QDockWidget 而不是嵌套 QSplitter", 2)

    para(doc,
         "需求要求「各面板支持拖拽调整大小与隐藏/显示切换」。"
         "QSplitter 能做前者，做不了后者（关掉之后没有入口找回来）。"
         "QDockWidget 两者都原生支持，还额外送了浮动与重新停靠。")

    code(doc,
         "addDockWidget (Left,   BOM树)\n"
         "splitDockWidget(BOM树 → 图元面板, Vertical)      // 左栏上下分\n"
         "addDockWidget (Right,  属性面板)\n"
         "splitDockWidget(属性面板 → 执行信息, Vertical)    // 右栏上下分\n"
         "addDockWidget (Bottom, 命令行)\n"
         "setCentralWidget(图形区)                         // 中间自动占满剩余空间")

    para(doc,
         "有一个次序陷阱：视图菜单里的「面板」子菜单要引用各 dock 的 toggleViewAction()，"
         "所以构造顺序必须是 buildDocks() → buildToolbar() → buildMenu()。"
         "初版顺序反了，菜单里拿到的是空指针。这类依赖不体现在类型系统里，"
         "只能靠在代码里写清楚为什么是这个顺序。")

    para(doc,
         "另外补了一个「全部显示」动作。用户把面板一个个关掉之后，"
         "逐个重新勾选是很烦的，一键恢复是低成本高收益的补充。")

    h(doc, "4.2  工具条：互斥高亮与快捷键", 2)

    para(doc,
         "绘图类工具（直线/圆/圆弧/矩形/多边形/样条/标注/约束/修剪/延伸）放进 QActionGroup 做互斥，"
         "视图类动作（缩放/网格/平面切换）不进组——因为它们是即时命令，执行完不改变模式。"
         "这个区分让「当前处于什么状态」在界面上永远只有一个答案。")

    para(doc,
         "关键在于高亮同步的方向：按钮不自己管自己的选中态，"
         "而是订阅 model_->activeToolChanged。这样无论工具是从工具条点的、"
         "快捷键按的、元素面板双击的，还是命令行触发的，高亮都一致。"
         "这正是第 2 章那条纪律的直接收益。")

    table(doc,
          ["快捷键", "动作", "快捷键", "动作"],
          [["L / C / A / R", "直线 / 圆 / 圆弧 / 矩形", "Ctrl+Z / Y", "撤销 / 重做"],
           ["P / S", "多边形 / 样条", "Ctrl+C / V", "复制 / 粘贴"],
           ["D / N", "标注 / 约束", "Ctrl+N/O/S", "新建 / 打开 / 保存"],
           ["T / X", "修剪 / 延伸", "Ctrl+E", "导出 DXF"],
           ["Esc", "中止绘制 / 回到选择", "Ctrl+1/2/3", "XY / YZ / XZ 平面"],
           ["Backspace", "回退上一个拾取点", "F7 / Ctrl+0", "栅格开关 / 适配窗口"],
           ["Enter", "闭合多段线与样条", "Ctrl+Enter", "焦点跳到命令行"],
           ["↑ / ↓", "命令历史", "Tab", "命令补全"]],
          widths=[3.4, 4.4, 3.4, 4.3])

    para(doc,
         "Esc 的语义做了分层：正在绘制时先中止当前绘制（保留工具不变，方便继续画下一个），"
         "没在绘制时才退回选择工具。一次按键解决两种诉求，符合主流 CAD 的肌肉记忆。"
         "Backspace 回退单个拾取点则是画多段线时的刚需——"
         "点错一个点不该被迫从头再来。")

    h(doc, "4.3  图形区的几个实现细节", 2)

    bullet(doc,
           "**栅格分级绘制**：1 mm 细线 + 10 mm 粗线，"
           "并在屏幕像素间距小于 3 px 时直接跳过该级绘制。"
           "缩小视图时如果照画不误，会退化成一片灰并拖垮帧率。")
    bullet(doc,
           "**缩放以光标为锚点**：先记下光标下的世界坐标，缩放后反解平移量使该点不动。"
           "以窗口中心为锚点实现更简单，但用户放大细节时目标会跑出视野。")
    bullet(doc,
           "**橡皮筋预览独立于正式图元**：预览用一个临时 ElementItem，"
           "不进模型、不触发 modelChanged、不进撤销栈。"
           "否则鼠标移动一次就污染一次撤销历史。")
    bullet(doc,
           "**切换工具时清理进行中状态**：订阅 activeToolChanged 调 resetPending()，"
           "清掉待定点、预览项和拾取缓存。这是画到一半切工具时的必要收尾。")

    doc.add_page_break()

    # ---------------- 5 ----------------
    h(doc, "5  工程化：构建与验证", 1)

    h(doc, "5.1  一个耗时的构建故障与它的真实成因", 2)

    para(doc,
         "开发中期 Qt 的 AUTOMOC 步骤持续失败，退出码 1 且没有有用的错误输出。"
         "第一直觉是 MSYS 的 /c/... 路径与 Windows 原生 C:/... 路径冲突"
         "——Ninja 通过 cmd.exe 调用编译器，PATH 形式不兼容会导致 DLL 找不到。"
         "沿这个方向排查了较长时间，包括把 PATH 换成 Windows 风格重新配置"
         "（结果连 CMake 自身都找不到编译器了）。")

    para(doc, "真实原因平淡得多：", bold=True)

    para(doc,
         "早期为验证工具链，写过一个最小的 MainWindow 冒烟测试放在 src/MainWindow.cpp。"
         "正式实现放在 src/ui/MainWindow.cpp 之后，CMakeLists 里的引用没改，"
         "仍然指向那个 341 字节的桩文件。于是 AUTOMOC 面对两个同名同命名空间的 "
         "MainWindow 类，行为未定义。删掉桩文件、修正引用，构建立刻通过。")

    para(doc, "这次排查留下三条可复用的经验：", bold=True)
    bullet(doc, "工具链层面的猜想要**先证伪再深入**——"
                "直接手工执行一次 autogen 的编译命令，发现它返回 0，"
                "本应立刻排除掉「路径不兼容」这个假设，而不是继续加固它；")
    bullet(doc, "脚手架代码要**当场清理**。临时文件活过它的用途就会变成陷阱；")
    bullet(doc, "「同名类」在 Qt 里比在纯 C++ 里更危险，因为 MOC 会生成额外的元对象代码，"
                "冲突不一定报在源头。")

    h(doc, "5.2  为什么专门做一个无界面自测目标", 2)

    para(doc,
         "GUI 程序难自动化验证。但把状态收敛到 SketchModel 之后，"
         "核心逻辑其实完全不依赖界面——命令解析、图元数据、撤销重做、剪贴板、"
         "BOM 排序、信号发射，全都可以在无窗口环境下跑。")

    para(doc,
         "于是加了第二个 CMake 目标 SketchSelfTest，只链接 core 层，"
         "覆盖 13 组共 57 项断言。这是分层架构给出的一个额外红利："
         "**当初为了解耦做的分层，顺带让核心逻辑变得可测**。")

    table(doc,
          ["测试组", "覆盖内容", "断言"],
          [["1  命令行绘图", "6 类图元的命令创建与计数", "6"],
           ["2  几何正确性", "端点、半径、角度、矩形对角点", "5"],
           ["3  非法输入", "缺参数、缺半径、未知命令、模型不被污染", "4"],
           ["4  撤销重做", "栈进出与命令式 undo/redo", "3"],
           ["5  复制粘贴删除", "剪贴板往返与计数一致性", "3"],
           ["6  BOM 重排", "首→末、末→首拖拽排序", "2"],
           ["7  视图命令", "平面切换、栅格三态、缩放边界", "8"],
           ["8  补全与历史", "前缀匹配、唯一匹配、空匹配、历史顺序", "6"],
           ["9  属性写入", "改名/图层/颜色、尺寸描述", "3"],
           ["10 信号联动", "三类信号确实被发射", "3"],
           ["11 约束标注", "合法落库、非法拒绝、类型正确", "6"],
           ["12 交点几何", "垂直/对角相交、平行、段内外判定", "6"],
           ["13 清空", "clear 后模型为空", "1"],
           ["合计", "—", "**57 / 57 通过**"]],
          widths=[3.6, 8.4, 3.5])

    para(doc,
         "自测在开发过程中直接抓出两个真问题：grid 命令不支持无参切换"
         "（工具条按钮与命令行行为不一致），以及 3.5 节那个拖拽类型缺陷。"
         "另有两处是测试期望本身写错，一并修正。")

    h(doc, "5.3  构建脚本", 2)

    para(doc,
         "把构建过程固化为 build.sh，四种模式覆盖日常需要，"
         "避免每次手敲一长串环境变量：")

    code(doc,
         "./build.sh          # 增量构建\n"
         "./build.sh clean    # 清理后完整重建\n"
         "./build.sh test     # 构建并运行 57 项自测\n"
         "./build.sh dist     # 构建并用 windeployqt 打包为独立可执行目录")

    para(doc,
         "脚本里注释了两个踩过的坑：CMake/Ninja 是原生 Windows 程序只认 C:/ 形式路径"
         "（用 pwd -W 转换），以及 Qt 在 MSYS 伪终端下默认不写 stderr"
         "（需要 QT_ASSUME_STDERR_HAS_CONSOLE=1，否则自测输出为空、只剩一个退出码）。"
         "这类环境知识不写下来，下次一定会再花一遍时间。")

    doc.add_page_break()

    # ---------------- 6 ----------------
    h(doc, "6  能力边界：做了什么，没做什么", 1)

    para(doc,
         "如实说明边界，比宣称「功能完备」更有价值。以下清单是刻意维护的。")

    table(doc,
          ["能力", "状态", "说明"],
          [["六类图元绘制", "已实现",
            "直线/圆/圆弧/矩形/多边形/样条，鼠标与命令行两条通路等价"],
           ["尺寸标注", "已实现",
            "两点距离标注，数值真实计算并写入图元"],
           ["直线修剪 / 延伸", "已实现",
            "两直线求交后移动端点，含段内/段外合理性校验"],
           ["几何约束", "关系已落库",
            "8 种约束类型的记录与引用完整；**求解器为规划项**，"
            "当前不做位置反解，日志中明确告知"],
           ["圆弧 / 样条的修剪", "未实现",
            "需要圆-线、参数曲线求交；当前明确提示「仅支持直线」而非静默失败"],
           ["撤销 / 重做", "已实现", "快照式，栈深 200"],
           ["文件存取", "已实现", "JSON 完整往返（含图层、颜色、引用关系）"],
           ["DXF 导出", "已实现",
            "LINE / CIRCLE / ARC / LWPOLYLINE；矩形展开为四条边"],
           ["工作平面", "已实现", "XY / YZ / XZ 切换，状态栏实时显示"],
           ["三维建模", "不在范围", "本模块只负责草图，属实体建模模块职责"]],
          widths=[3.4, 2.6, 9.5])

    para(doc,
         "「未实现」与「不在范围」被分开写。前者是承认能力缺口并已在交互中给出提示，"
         "后者是职责划分。把两者混为一谈会让读者高估或低估模块状态。",
         italic=True, color=GREY)

    h(doc, "6.1  下一步的优先级判断", 2)

    bullet(doc, "**几何约束求解器**（高）。这是草图模块从「画图工具」升级为「参数化设计工具」"
                "的分界线。需要牛顿迭代 + 欠/过约束诊断，工作量最大但价值也最高。")
    bullet(doc, "**对象捕捉**（高）。端点/中点/交点/圆心捕捉是精确绘图的前提，"
                "投入产出比在所有待办里最好。")
    bullet(doc, "**曲线求交**（中）。补齐圆弧与样条的修剪延伸，让编辑工具覆盖完整。")
    bullet(doc, "**大规模场景优化**（中）。图元过万时需要空间索引（R-Tree / 四叉树），"
                "当前 rebuild() 全量重建会成为瓶颈。这也是把撤销从快照换成增量 diff 的时点。")
    bullet(doc, "**图层管理面板**（低）。数据结构里 layer 字段已预留，缺的只是界面。")

    doc.add_page_break()

    # ---------------- 7 ----------------
    h(doc, "7  回头看：哪些判断是对的", 1)

    para(doc, "**站得住的决策**", bold=True)

    bullet(doc, "把问题从「搭界面」重述为「解耦输入与展示」。"
                "这一步定了调，后面所有细节都有了判断依据——"
                "遇到「要不要让 A 直接调 B」这类问题时，答案永远是不要。")
    bullet(doc, "用快照换撤销的正确性。开发全程没有出现过一次撤销相关的数据异常。")
    bullet(doc, "把核心层做成不依赖 Qt Widgets 的纯逻辑。"
                "这本是为解耦，结果直接换来了 57 项可自动化回归的断言。")
    bullet(doc, "信号拆细而不是合并。cursorMoved 与 modelChanged 分开，"
                "让高频事件不触发重建，交互流畅度是这么保住的。")

    para(doc, "**应该更早做对的**", bold=True)

    bullet(doc, "冒烟测试的桩文件没有当场删除，最终导致一次冗长的构建故障排查。"
                "临时代码的生命周期应当显式管理。")
    bullet(doc, "trim/extend/dim 初版一律标记「待接入」，是对难度的错误估计。"
                "标注只是一个 sqrt，直线求交是初等几何。"
                "**「暂不实现」应当是评估之后的结论，不能是评估的替代品**。")
    bullet(doc, "拖拽类型的中英文错配靠人工走查才发现，说明跨模块的标识约定"
                "应该在编码时就用类型系统固定（例如枚举而非裸字符串），"
                "而不是靠事后检查。")

    doc.add_paragraph()
    p = doc.add_paragraph()
    r = p.add_run(
        "本模块共 26 个源文件约 2,300 行代码，编译零警告，57 项自测全部通过。"
        "架构上的核心投资是「单一数据源 + 信号广播」——"
        "它让三种输入方式与六个展示组件之间保持零耦合，"
        "使得新增工具、新增面板都是加法而非乘法。"
        "这份文档记录的判断过程，比代码本身更值得留存。")
    r.font.size = Pt(10.5)
    r.italic = True
    r.font.color.rgb = GREY
    set_cn(r)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    doc.save(OUT)
    print("saved:", OUT)


if __name__ == "__main__":
    build()
