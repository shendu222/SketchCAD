#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《直线绘制-参数输入条与预览-深度思考》docx。

覆盖本次 5 项需求：
  1) 工具条「直线」菜单中文化 + 二级子菜单
  2) 水平/垂直线实时预览
  3) 参数输入条（角度/长度、间隔/条数）
  4) parallel 多偏移平行线
  5) parallel through point 等长对齐
  以及「其他直线绘制方法检查」与全量自测结论（174 通过 / 0 失败）。
"""
import os
from docx import Document
from docx.shared import Pt, RGBColor, Cm
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH

FONT = "Microsoft YaHei"
DOC = "D:/01-CAD/02-avticationCAD/sketch-cad-cpp/docs/直线绘制参数输入条与预览-深度思考.docx"

os.makedirs(os.path.dirname(DOC), exist_ok=True)
doc = Document()

# 默认字体
style = doc.styles['Normal']
style.font.name = FONT
style._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
style.font.size = Pt(10.5)

# 标题
T = doc.add_heading("航空CAD-草图绘制模块-深度思考", level=0)
T.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = T.runs[0]
r.font.name = FONT
r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
r.font.size = Pt(18)
r.font.bold = True

SUB = doc.add_paragraph("直线绘制增强：菜单中文化与二级子菜单 · 水平/垂直实时预览 · 参数输入条 · 平行线与过点平行线")
SUB.alignment = WD_ALIGN_PARAGRAPH.CENTER
SUB.runs[0].font.name = FONT
SUB.runs[0]._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
SUB.runs[0].font.size = Pt(12)
SUB.runs[0].font.color.rgb = RGBColor(0x40, 0x40, 0x40)

def P(text, bold=False, size=10.5):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.font.name = FONT
    r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
    r.font.size = Pt(size)
    r.font.bold = bold
    return p

def H(text, level=1):
    h = doc.add_heading(level=level)
    r = h.add_run(text)
    r.font.name = FONT
    r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
    r.font.color.rgb = RGBColor(0x1A, 0x1D, 0x28)
    return h

def bullet(text, level=0):
    p = doc.add_paragraph(style='List Bullet' if level == 0 else 'List Bullet 2')
    r = p.add_run(text)
    r.font.name = FONT
    r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
    return p

def code(text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(1)
    p.paragraph_format.space_after = Pt(6)
    r = p.add_run(text)
    r.font.name = "Consolas"
    r._element.rPr.rFonts.set(qn('w:eastAsia'), "Consolas")
    r.font.size = Pt(9)
    r.font.color.rgb = RGBColor(0x2B, 0x59, 0xA8)
    return p

# ===================== 正文 =====================
H("一、需求理解与总览", 1)
P("本次在已实现的 18 种直线绘制方式（LineModes）之上，进一步补充交互与参数化能力，共 5 项需求，"
  "并要求复核其余直线画法是否正常。逐条拆解：")
bullet("菜单中文化：工具条「直线」下拉项改为中文，并作为二级子菜单挂到顶层【直线(&L)】菜单下，与 18 种方式一一对应。")
bullet("水平/垂直实时预览：拖动过程中实时渲染水平线 / 垂直线预览，而非仅到 release 才出线。")
bullet("参数输入条：工具条下方新增输入条；【angle】可输入角度（与 X 轴夹角）和长度，选起点后直接成线；"
      "【parallel】可输入间隔与条数（默认 1 条），选参考线后按方向偏移成平行线组。")
bullet("parallel 实现：沿参考线垂直方向、按间隔绘制多条、长度与参考线相同的平行线；点击方向决定偏移侧。")
bullet("parallel through point 实现：选参考直线后，拾取或命令行输入一点，绘制与参考线平行、等长、且在该点垂向对齐位置的直线。")
bullet("其他直线方法复核：逐一检查其余直线画法，并将思考过程导出为本文件。")

H("二、核心设计原则", 1)
bullet("模型为参数唯一真源：新增 SketchModel::LineDrawParams（angleDeg/length/spacing/count），"
      "参数输入条写入模型，绘制命令读取模型——工具条、菜单、命令行、参数条四条通路同源，避免状态不一致。")
bullet("几何与 UI 解耦（延续）：angle / parallel / parallel_through_point 的纯几何计算下沉到 src/core/LineModes，"
      "无界面自测可直接验证，不依赖鼠标事件。")
bullet("预览即「用光标算一次」：updatePreview 用当前光标作为 cur 调用同一 compute 入口，所见即所得。")
bullet("方向由鼠标侧决定：parallel 多条偏移用 sidePoint 在参考线垂直轴的有符号投影决定整体 sign，点哪侧偏哪侧。")
bullet("向后兼容：原有 line / line_xxx 工具 id 不变；参数条仅在对应模式启用相关字段，不影响其它模式。")

H("三、需求1 — 菜单中文化与二级子菜单", 1)
P("3.1 工具条下拉中文化", bold=True)
P("原工具条「直线」QToolButton 下拉里是英文 id（2 Points / Horizontal / Vertical ...）。"
  "全部替换为中文显示文案，但 QAction 的 data/objectName 仍用稳定 id，保证快捷键与信号绑定不受影响。")
code("两点直线 / 水平线 / 竖直线 / 角度直线 / 平行线 / 过点平行线\n"
     "/ 角平分线 / 点-圆切线 / 圆-圆公切线 / 垂直参考线切线 / 垂直线 / 相对角度线 / 正交折线 ...")
P("3.2 顶层【直线(&L)】菜单与二级子菜单", bold=True)
P("新增 buildLineMenu()，把 18 种方式按语义拆成 4 个二级子菜单，挂在【直线(&L)】下，与工具条下拉共享同一组 toolActions_ 指针，"
  "因此三处入口的 checked 状态、快捷键、触发槽完全同步，不会出现「菜单选了但按钮不高亮」的错位。")
bullet("基本：两点直线、水平线、竖直线、角度直线、平行线、过点平行线")
bullet("切线：点-圆切线、圆-圆公切线、垂直参考线切线")
bullet("垂直与角度：角平分线、垂直线、相对角度线")
bullet("正交折线：正交折线（含 Snake X/Y 变体）")
P("复用 toolActions_ 是本次的关键：菜单栏【草图绘制】、工具条下拉、【直线】菜单三者指向同一 QAction 实例，"
  "QActionGroup 保证单选互斥，状态天然一致。")

H("四、需求2 — 水平/垂直线实时预览", 1)
P("4.1 触发路径", bold=True)
P("GraphicsView::mouseMoveEvent 中新增分支：只要当前是直线子工具（isLineSubTool(model_->activeTool())），"
  "就调用 updatePreview(snapWorld(w)) 并 return，使得尚未拾取到起点时也能跟手预览。")
code("if (isLineSubTool(model_->activeTool())) {\n"
     "    updatePreview(snapWorld(w));\n"
     "    return;\n"
     "}")
P("4.2 预览几何", bold=True)
P("起点 a 在第一下按下时锁定；拖动时光标 cur，把 b 约束为：")
bullet("水平线：b = Vec3(cur.x, a.y, 0)  —— y 固定为起点 y，仅 x 跟随光标。")
bullet("竖直线：b = Vec3(a.x, cur.y, 0)  —— x 固定为起点 x，仅 y 跟随光标。")
P("这样在拖动阶段就能看到一条「齐平」的 H 线或 V 线随光标伸缩，release 时按预览端点落线。"
  "该约束只发生在预览分支，最终 addLineSegment 仍用真实落点，保证无吸附误差累积。")

H("五、需求3 — 参数输入条", 1)
P("5.1 布局", bold=True)
P("MainWindow 中央区由「单 GraphicsView」改为「QVBoxLayout 容器」，上放 paramBar_、下放 view_。参数条本身是一行控件：")
bullet("角度 (QDoubleSpinBox sbAngle_，单位°，范围 -360~360)")
bullet("长度 (QDoubleSpinBox sbLength_)")
bullet("间隔 (QDoubleSpinBox sbSpacing_)")
bullet("条数 (QSpinBox sbCount_，默认 1)")
bullet("提示标签 paramHint_（随当前命令变化，如「选择起点」「选择参考线」）")
P("5.2 写入模型", bold=True)
P("buildParamBar() 为每个 spin box 连接 valueChanged → 调用 SketchModel 对应 setter "
  "(setLineParamAngle/Length/Spacing/Count)。模型内部做「相等即跳过 + emit lineParamsChanged()」，"
  "天然支持外部（如命令行）改参后输入框同步刷新。")
code("connect(sbAngle_,  &QDoubleSpinBox::valueChanged, model_, &SketchModel::setLineParamAngle);\n"
     "connect(sbLength_, &QDoubleSpinBox::valueChanged, model_, &SketchModel::setLineParamLength);\n"
     "connect(sbSpacing_,&QDoubleSpinBox::valueChanged, model_, &SketchModel::setLineParamSpacing);\n"
     "connect(sbCount_,  &QSpinBox::valueChanged,        model_, &SketchModel::setLineParamCount);")
P("5.3 按命令启停字段", bold=True)
P("updateParamBarForTool(t)：依据当前工具 id 启用/禁用对应字段并显示提示，避免无关字段误导用户：")
bullet("line_angle：启用 角度 + 长度，提示「点击起点」")
bullet("line_parallel：启用 间隔 + 条数，提示「选择参考线，点击偏移方向」")
bullet("其余直线模式：全部禁用，仅提示当前操作")
P("activeToolChanged 信号里调用 updateParamBarForTool，切换工具即自动切换参数条形态。")

H("六、需求3/4 — angle 与 parallel 命令逻辑", 1)
P("6.1 角度直线（单点 + 参数）", bold=True)
P("handleLineSubTool 中 Angle 改为「单点」流程：拾取起点 a 后，读取模型 angleDeg / length，"
  "用 LineModes::pointAtAngleLength 直接算出终点 b 并落线，不需要第二点。公式：")
code("b = a + length * (cos(angleDeg*π/180), sin(angleDeg*π/180), 0)")
P("预览阶段（未拾取起点的拖动）则用 line_angle 分支把光标当起点，实时显示按当前参数的射线。")
P("6.2 平行线（选参考线 → 点方向 → 多偏移）", bold=True)
P("Parallel 流程：先拾取参考线 refLine；再点一次 sidePoint 决定偏移方向；"
  "调用 LineModes::parallelCopies(refLine, sidePoint, spacing, count) 生成 count 条等长平行线。")
bullet("方向 sign：取 sidePoint 在参考线垂直单位向量 n 上的有符号投影 t = (sidePoint−a)·n；t≥0 取 +1，否则 −1。")
bullet("逐条偏移：对 k=1..count，生成端点 a + n*(sign*k*spacing) 与 b + n*(sign*k*spacing)，长度与参考线一致。")
bullet("默认 count=1：仅生成一条偏移线，与「过点平行线」视觉一致但语义不同（本功能按间隔，过点按对齐点）。")

H("七、需求5 — parallel through point（等长对齐）", 1)
P("7.1 与 parallel 的区别", bold=True)
P("parallel 按「固定间隔」偏移；parallel through point 按「对齐点」偏移——把参考线整体平移，"
  "使直线落在参考线垂直方向上与该点对齐的位置，长度与参考线完全相同。")
P("7.2 几何公式", bold=True)
P("LineModes::parallelAligned(refLine, p)：取参考线方向 u=(b−a)，垂直单位向量 n=perp(u)/|perp(u)|；"
  "计算有符号投影 t = (p−a)·n（即 p 相对参考线在垂直方向的偏移量），整条线平移该量：")
code("t  = dot2(p - a, n);\n"
     "A' = a + n * t;\n"
     "B' = b + n * t;")
P("这样新线的中垂线必然经过 p（在垂直方向上与 p 对齐），且 |B'−A'| = |b−a| 保证等长。")
P("7.3 交互与命令行", bold=True)
bullet("鼠标：选参考线 → 再点（或画布上任意位置）一点 p，即落线。")
bullet("命令行：line_parallel_pt <refId> x,y（CommandInterpreter 拆分为 parallelAligned，仅取 x,y 一点，"
      "区别于 parallel 的两点定位）。")

H("八、几何实现落点（LineModes）", 1)
P("src/core/LineModes.h/.cpp 新增与调整：")
bullet("Vec3 pointAtAngleLength(const Vec3& a, double angleDeg, double length)")
bullet("Result parallelAligned(const Element& refLine, const Vec3& p)")
bullet("struct ParallelCopies { bool valid; std::vector<std::pair<Vec3,Vec3>> lines; QString msg; }")
bullet("ParallelCopies parallelCopies(const Element& refLine, const Vec3& sidePoint, double spacing, int count)")
bullet("compute() 内将 Parallel 与 ParallelThroughPoint 拆分：Parallel 走 parallelThroughPoint（过 pts[0]，长度由 pts[1] 定）；"
      "ParallelThroughPoint 走 parallelAligned（等长对齐）。")
P("所有纯几何函数不依赖任何 UI 类型，因此可在无界面自测中直接调用验证。")

H("九、UI / 模型 / 命令行改动清单", 1)
bullet("SketchModel.h/.cpp：LineDrawParams 结构 + 4 个 setter + lineParams() 访问器 + lineParamsChanged() 信号 + 私有成员 lp_。")
bullet("GraphicsView.cpp：删除未使用的 gvPerp/gvNorm/gvDot/gvRefLine 静态函数（消除 -Wunused-function）；"
      "updatePreview 新增 line_angle / line_parallel / line_parallel_pt / line_h / line_v 预览；"
      "mouseMoveEvent 增加 isLineSubTool 预览分支；handleLineSubTool 重写 Angle 单点、Parallel 多偏移与分组。")
bullet("CommandInterpreter.cpp：拆分 Parallel（parallelThroughPoint，需 x1,y1 x2,y2）与 ParallelThroughPoint（parallelAligned，仅 x,y）。")
bullet("MainWindow.h/.cpp：中央容器布局；直线下拉中文化；【直线(&L)】菜单 + buildLineMenu 4 个二级子菜单；"
      "buildParamBar / updateParamBarForTool；activeToolChanged 接 updateParamBarForTool；新增 #include <QSpinBox> <QDoubleSpinBox>。")

H("十、其他直线绘制方法复核", 1)
P("在增加本次功能的同时，对 18 种直线模式逐一回归，确认未因改动（尤其 mouseMoveEvent 分支、handleLineSubTool 分组、"
  "compute 的 Parallel 拆分）而受影响：")
bullet("两点 / 水平 / 垂直 / 角度：H/V 预览已验证；angle 单点流程替代原「需第二点」，其余模式逻辑不变。")
bullet("平行线（原 parallel）：改为 parallelCopies 多偏移后，count=1 行为与旧版单条一致，未回归。")
bullet("过点平行线：改用 parallelAligned 等长对齐，比原「过点 + 第二点定长」更直观且等长。")
bullet("切线族（点-圆、圆-圆、垂直参考线）：未触碰，依赖既有 getRefLine / 切线公式。")
bullet("角平分线 / 正交折线 Snake：未触碰，依赖既有交点与方向翻转逻辑。")
bullet("相对角度线 / 正交折线变体：分组调整未改变其 enough 判定（仍需相应点数）。")
P("结论：其余直线画法行为保持稳定，无新增回归。")

H("十一、自测覆盖", 1)
P("tests/selftest.cpp 新增 §23（7 项），与既有 §22（18 种方式）共同构成直线回归：")
bullet("pointAtAngleLength：30°、长度 100 的终点坐标校验。")
bullet("parallelAligned：参考线 (0,0)-(100,0)，过点 (50,20) → 期望 (0,20)-(100,20) 等长对齐。")
bullet("parallelCopies 正向：间隔 20、count 3，sidePoint 在上方 → 向上生成 3 条。")
bullet("parallelCopies 负向：sidePoint 在下方 → sign 翻转，向下生成 3 条。")
bullet("CLI line_parallel_pt：独立 SketchModel ml，参考线 id=1，过点平行线 id=2，断言 ml.element(2) 正确。")
bullet("其它 §23 子项：边界与消息字段（valid / msg）校验。")
P("全量自测结果：**174 项通过 / 0 失败**（原 167 + 新增 7），CMake 构建 **0 错误 / 0 警告**"
  "（构建前已将 Qt bin 加入 PATH：export PATH=\"/c/Qt/6.10.1/mingw_64/bin:$PATH\"）。")

H("十二、代码落点总览", 1)
bullet("src/core/LineModes.h / .cpp：pointAtAngleLength / parallelAligned / parallelCopies 及 compute 拆分")
bullet("src/core/SketchModel.h / .cpp：LineDrawParams 与 setter / 信号")
bullet("src/ui/GraphicsView.h / .cpp：isLineSubTool / handleLineSubTool 重写、updatePreview 新增预览、mouseMoveEvent 预览分支")
bullet("src/ui/MainWindow.h / .cpp：中文化下拉、buildLineMenu 二级子菜单、buildParamBar / updateParamBarForTool、中央容器布局")
bullet("src/core/CommandInterpreter.cpp：Parallel / ParallelThroughPoint 命令行拆分")
bullet("tests/selftest.cpp：§23 参数条与平行算法回归")
bullet("tools/gen_deepthink_lineparams.py：本文件生成脚本")

H("十三、已知简化与后续扩展", 1)
bullet("参数条目前只覆盖 angle（角度+长度）与 parallel（间隔+条数）；其余模式（如相对角度）后续可接入角度字段。")
bullet("parallelCopies 仅实现「单侧偏移」：点击方向决定正/负，但不支持「两侧对称」（如各 N 条）；可作为选项扩展。")
bullet("过点平行线目前整条平移对齐；若需「从该点起按参考线方向延伸定长」，可在参数条追加起点模式。")
bullet("参数条与属性面板未来可合并为统一的命令参数区，进一步统一工具条 / 菜单 / 命令行 / 参数条四入口。")

# 页脚时间
P("")
pf = P("文档生成时间：2026-08-05", size=9)
pf.runs[0].font.color.rgb = RGBColor(0x80, 0x80, 0x80)

doc.save(DOC)
print(f"Saved: {DOC}  ({os.path.getsize(DOC)} bytes)")
