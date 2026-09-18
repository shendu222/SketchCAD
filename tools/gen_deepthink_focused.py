# -*- coding: utf-8 -*-
"""生成《航空CAD-草图绘制模块-深度思考（框选多选与选区智能标注）》聚焦文档。

该文档仅导出最近一轮「框选多选 + 选区智能标注」两项修改的设计推演，
与既有综合文档（四项增强）并列，便于单独核查本轮改动。
"""
from docx import Document
from docx.shared import Pt, RGBColor, Inches
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT

OUT = r"D:/01-CAD/02-avticationCAD/docs/航空CAD-草图绘制模块-深度思考（框选多选与选区智能标注）.docx"

DOC_TITLE = "航空CAD 草图绘制模块 — 深度思考（框选多选与选区智能标注）"
SUBTITLE = "C++17 / Qt6 实现 · 橡皮筋框选 · 选择集下沉模型 · 选区驱动智能标注"

doc = Document()

# ---------- 基础样式 ----------
normal = doc.styles["Normal"]
normal.font.name = "Microsoft YaHei"
normal.font.size = Pt(10.5)

def H1(text):
    return doc.add_heading(text, level=1)

def H2(text):
    return doc.add_heading(text, level=2)

def P(text, bold=False, italic=False):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.bold = bold
    r.italic = italic
    return p

def bullet(text):
    p = doc.add_paragraph(style="List Bullet")
    p.add_run(text)
    return p

def numbered(text):
    p = doc.add_paragraph(style="List Number")
    p.add_run(text)
    return p

def code(text):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.font.name = "Consolas"
    r.font.size = Pt(9)
    r.font.color.rgb = RGBColor(0x1F, 0x49, 0x7D)
    return p

def table(headers, rows):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Light Grid Accent 1"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, h in enumerate(headers):
        c = t.rows[0].cells[i]
        c.text = ""
        run = c.paragraphs[0].add_run(h)
        run.bold = True
    for row in rows:
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = str(v)
    return t

# ============================================================
# 封面 / 概述
# ============================================================
title = doc.add_paragraph()
title.alignment = WD_ALIGN_PARAGRAPH.CENTER
tr = title.add_run(DOC_TITLE)
tr.bold = True
tr.font.size = Pt(19)
tr.font.color.rgb = RGBColor(0x1A, 0x1D, 0x28)

sub = doc.add_paragraph()
sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
sr = sub.add_run(SUBTITLE)
sr.italic = True
sr.font.size = Pt(10.5)
sr.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

doc.add_paragraph()
P("本文档聚焦 sketch-cad-cpp（C++17 / Qt 6.10.1 / MinGW-w64 GCC 13.1.0）最近一轮两项修改的设计推演："
  "① 鼠标框选（橡皮筋）多选图元；② 依据当前选区智能生成标注（选中直线→对齐标注、选中圆→直径标注）。"
  "重点记录「为什么这样设计」「放弃了哪些替代方案」，每一节结论都对应一处可在源码核查的落点。"
  "更庞大的架构基线（坐标翻转、undo 粒度、dim::build 唯一生成器等）已在综合文档中阐述，此处仅引用不复述。")

P("验证基线：干净构建 0 警告 0 错误；核心自测 125 项全绿，其中 §18 多选集合 + 智能标注共 16 项用例专门覆盖本轮修改。",
  bold=True)

# ============================================================
# 1. 框选多选
# ============================================================
H1("一、多选支持与鼠标框选（橡皮筋）")
P("目标：用户可通过鼠标从起点拖到终点形成对角矩形，框内的所有图元一次性选中；并支持 Ctrl 加选/减选，"
  "使选择集可累积编辑（如批量移色、批量删除、批量加约束）。")

H2("1.1 选择集下沉到模型，而非视图")
P("最容易踩的坑是把「多选状态」当成视图私有变量。一旦选中集只活在 GraphicsView 里，"
  "属性面板、约束、剪切等所有依赖 selection 的通路都得反向问视图要数据，耦合立刻乱掉。"
  "正确做法是让 SketchModel 持有 selectedIds_ 选择集：")
bullet("setSelection / addToSelection / toggleSelection / clearSelection 统一改写选择集并 emit selectionChanged。")
bullet("selectedId_ 作为「主选择」（属性面板编辑对象、约束单对象操作）退化为选择集的末尾元素，向后兼容旧调用。")
bullet("removeElement / replaceElement / undo / redo / clear 都同步维护 selectedIds_，杜绝「选中了一个已被删除的 id」这类悬空引用。")

P("落点（SketchModel.h / .cpp）：")
code("QSet<int> selectedIds_;            // 选择集\n"
     "int       selectedId_ = -1;       // 主选择 = selectedIds_ 末尾\n"
     "void setSelection(const QList<int>& ids);\n"
     "void addToSelection(const QList<int>& ids);\n"
     "void toggleSelection(int id);\n"
     "void clearSelection();\n"
     "bool isSelected(int id) const;\n"
     "bool hasSelection() const;\n"
     "QList<int> selection() const;")

H2("1.2 框选命中的几何判据")
P("判定「图元是否落在框内」用包围盒相交（r.intersects(elementSceneBounds(el))），而 elementSceneBounds "
  "复用 ElementItem::boundingRect 的几何包围盒——这意味着框选的命中范围与屏幕上画出来的几何范围严格一致，"
  "不会出现「画出来在框里却选不中」或「没碰到却选中了」的错位。对标注图元，包围盒还纳入了尺寸线/界线/引线，"
  "因为用户框住标注的引线时本就该选中它。")

bullet("elementSceneBounds 通过构造一个临时 ElementItem 取其 boundingRect，零重复几何代码。")
bullet("框选只在释放时结算一次选中集，拖动中仅刷新 drawForeground 的虚线矩形 + 半透明填充，避免逐帧重建场景。")

H2("1.3 交互细节（手感来自边界条件）")
bullet("空白处按下即进入框选；若释放时矩形过小（宽高均小于拾取容差），视为「空白单击」→ 清空选择（Ctrl 除外）。")
bullet("Ctrl+元素点击 = toggleSelection（有则减、无则加），普通点击 = 单选，与主流 CAD 一致。")
bullet("Esc / 调用 resetPending 会同时撤销正在进行的框选，状态机不残留。")
bullet("rebuild() 高亮所有选中项（it->highlight = model_->isSelected(e.id)），不再是单主选择高亮。")

H2("1.4 设计呼应")
table(
    ["设计决定", "呼应点", "收益"],
    [["selectedIds_ 在模型", "需求3 联动/需求4 剪切", "选择/删除/约束共用同一来源，无耦合"],
     ["包围盒相交判据", "rebuild 绘制", "命中范围 = 绘制范围，零错位"],
     ["一次性结算", "drawForeground 虚线框", "框选不卡顿，状态机干净"]],
)

# ============================================================
# 2. 选区智能标注
# ============================================================
H1("二、依据选区自动生成标注（直线→对齐、圆→直径）")
P("目标：优化标注流程——当用户已选中一条直线，标注应沿该直线的垂直方向进行标注；当已选中一个圆，"
  "标注应显示其直径。本质是「让标注工具读懂当前选区，省去逐点拾取」。")

H2("2.1 「沿垂直方向标注」究竟指什么")
P("这里有个必须澄清的语义陷阱：若把「尺寸线」本身画成垂直于直线，则测量的两端点在垂直轴投影上完全重合，"
  "实测值为 0，毫无意义。国标里「垂直」指的是尺寸界线（extension line）垂直于被测线段——而这正是「对齐标注」"
  "的天然形态：尺寸线平行于直线（量真实长度），尺寸界线垂直于直线。因此「直线→沿垂直方向标注」被落地为"
  "对齐标注（Aligned），垂直方向即尺寸界线的方向。验证用例断言：选中 (0,0)→(100,0) 的直线，智能标注实测=100mm、dtype=Aligned。")

code("int SketchModel::smartDimension() {\n"
     "    auto sel = selection();\n"
     "    if (sel.size() != 1) return -1;          // 无/多选区 → 降级\n"
     "    const Element& e = element(sel.first());\n"
     "    if (e.type == Line) {                     // 直线 → 对齐标注\n"
     "        auto g = dim::build(Aligned, e.pts[0], e.pts[1], Vec3());\n"
     "        addElement([&]{ Element d; d.type=Dimension; d.dtype=Aligned;\n"
     "                        d.pts={e.pts[0],e.pts[1]}; d.measured=g.measured; return d; });\n"
     "    } else if (e.type == Circle) {            // 圆 → 直径标注\n"
     "        auto g = dim::build(Diameter, e.pts[0], Vec3(), Vec3(e.radius,0,0));\n"
     "        addElement([&]{ Element d; d.type=Dimension; d.dtype=Diameter;\n"
     "                        d.pts={e.pts[0]}; d.radius=e.radius; d.measured=g.measured; return d; });\n"
     "    } else return -1;                          // 不支持 → 降级\n"
     "    return lastId;\n"
     "}")

H2("2.2 圆→直径的统一落点")
P("选中圆时直接建 Diameter 标注：尺寸线通过圆心、两端各一箭头，标签前缀 ⌀（U+2300）。"
  "这与需求2里已实现的 dim::build(Diameter) 完全复用，无新增几何代码——又一次印证「core 层几何下沉」的价值："
  "新的交互入口只是换了个触发条件（选区而非逐点），底层生成器一字未改。")

H2("2.3 触发时机与降级（关键状态机）")
bullet("任一标注工具（线性/对齐/水平/竖直/智能标注 dim_smart）被激活时，若当前恰好选中单条直线或圆，直接 smartDimension() 生成，免去拾取。")
bullet("半径/角度类标注依赖显式点选（需点圆上的方位或角的顶点），不触发智能生成，避免误判。")
bullet("无选区 / 多选区 / 选区为不支持的图元（如矩形）→ smartDimension 返回 -1，调用方无缝回退到常规逐点拾取流程。")
bullet("智能标注同样走 addElement + 单次撤销，与既有标注创建路径一致；新增 dim_smart 工具（菜单「智能标注」）与 CLI dims 命令便于复现与自测。")

H2("2.4 自测落点（§18）")
bullet("多选 API：setSelection 后再 isSelected 返回真；toggleSelection 可加可减；replaceElement 后选择集自动剔除被替换 id。")
bullet("框选命中：构造两个不相交矩形，elementSceneBounds 相交判据正确区分框内/框外。")
bullet("智能标注：直线选区→Aligned 且实测=100；圆选区→Diameter 且 measured=2*radius；多选区/无选区/不支持图元→返回 -1 降级。")

# ============================================================
# 3. 验证结论
# ============================================================
H1("三、验证结论")
bullet("干净构建（CMake + Ninja + MinGW GCC 13.1.0）：0 警告 / 0 错误。")
bullet("核心自测 SketchSelfTest：125 项通过 / 0 失败；本轮新增 §18（多选集合 + 智能标注）共 16 项用例。")
bullet("主要改动文件：src/core/SketchModel.h/.cpp、src/ui/GraphicsView.cpp/.h、src/ui/MainWindow.cpp、src/core/CommandInterpreter.cpp、tests/selftest.cpp。")
bullet("本思考文档与综合文档《航空CAD-草图绘制模块-深度思考过程（四项增强 + 后续优化）》互补：综合文档覆盖架构基线与前四项增强，本文档聚焦最近一轮的框选多选与选区智能标注。")

doc.add_paragraph()
end = doc.add_paragraph()
er = end.add_run("—— 聚焦文档由深度思考过程自动导出（sketch-cad-cpp / C++17 / Qt6）")
er.italic = True
er.font.size = Pt(8.5)
er.font.color.rgb = RGBColor(0x88, 0x88, 0x88)

doc.save(OUT)
print("SAVED:", OUT)
