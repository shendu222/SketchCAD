# -*- coding: utf-8 -*-
"""生成《图形绘制系统智能捕捉点功能说明》Word 文档。
内容对应一次关于 object snap（智能捕捉）实现的完整讲解：
类型定义、算法实现、容差策略、多候选优先级、触发方式、代码落点。
注意：中文文案字符串统一用单引号包裹，避免与中文引号混淆。
"""
import os
from docx import Document
from docx.shared import Pt, RGBColor, Cm
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH

FONT = "宋体"
CODE_FONT = "Consolas"

doc = Document()

# ---- 统一中文字体 ----
def set_cjk(style_name, size=None, bold=None):
    st = doc.styles[style_name]
    st.font.name = FONT
    st.element.rPr.rFonts.set(qn("w:eastAsia"), FONT)
    if size is not None:
        st.font.size = Pt(size)
    if bold is not None:
        st.font.bold = bold

set_cjk("Normal", 11)
for hs, sz in (("Title", 20), ("Heading 1", 15), ("Heading 2", 13), ("Heading 3", 12)):
    set_cjk(hs, sz)

def para(text='', style=None, bold=False, italic=False, size=None, color=None, align=None):
    p = doc.add_paragraph(style=style)
    if align is not None:
        p.alignment = align
    if text:
        r = p.add_run(text)
        r.font.name = FONT
        r._element.rPr.rFonts.set(qn("w:eastAsia"), FONT)
        r.bold = bold
        r.italic = italic
        if size:
            r.font.size = Pt(size)
        if color:
            r.font.color.rgb = color
    return p

def code(text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Pt(12)
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(6)
    for ln in text.split('\n'):
        r = p.add_run(ln + '\n')
        r.font.name = CODE_FONT
        r._element.rPr.rFonts.set(qn('w:eastAsia'), CODE_FONT)
        r.font.size = Pt(9.5)
        r.font.color.rgb = RGBColor(0x33, 0x33, 0x33)
    return p

def bullet(text, level=0):
    p = doc.add_paragraph(style='List Bullet' if level == 0 else 'List Bullet 2')
    r = p.add_run(text)
    r.font.name = FONT
    r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
    return p

FIG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'docs', 'figures')

def add_figure(name, caption, width_cm=15.5):
    """插入图片（居中）与说明（居中、灰色小字）。"""
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run()
    r.add_picture(os.path.join(FIG_DIR, name), width=Cm(width_cm))
    cap = doc.add_paragraph()
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    cr = cap.add_run(caption)
    cr.font.name = FONT
    cr._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
    cr.italic = True
    cr.font.size = Pt(10)
    cr.font.color.rgb = RGBColor(0x66, 0x66, 0x66)
    doc.add_paragraph()  # 图后空行

# ===================== 标题 =====================
para('图形绘制系统中的图元绘制智能捕捉点功能说明',
     style='Title', align=WD_ALIGN_PARAGRAPH.CENTER)
para('对象捕捉（Object Snap）机制：类型定义 · 算法实现 · 容差策略 · 优先级 · 触发方式',
     style='Subtitle', align=WD_ALIGN_PARAGRAPH.CENTER, size=11,
     color=RGBColor(0x66, 0x66, 0x66))
para('说明文档  |  基于 sketch-cad-cpp 项目（C++17 / Qt6）的 Snap 模块实现',
     size=9.5, color=RGBColor(0x99, 0x99, 0x99), align=WD_ALIGN_PARAGRAPH.CENTER)
doc.add_paragraph()

# ===================== 引言 =====================
para('智能捕捉（object snap）的本质，是把“鼠标所在的自由世界坐标”替换为“离它最近的、'
     '有几何意义的图上特征点”。下文按五个维度逐层说明，并结合本项目 src/core/Snap.h/.cpp '
     '与 src/ui/GraphicsView.cpp 给出具体实现。')
para('一个关键设计原则：几何计算与 UI 完全解耦。纯几何层 snap::snap() 只负责“在容差内”'
     '找最近特征点，视图层只负责把它画出来、并把拾取点替换掉。这使得同一套几何逻辑可被'
     '视图、命令行与无界面自测共用。')

# ===================== 一、类型定义 =====================
para('一、捕捉点的类型定义', style='Heading 1')
para('智能捕捉的核心是“把鼠标自由点替换为有几何意义的特征点”。常见类型如下：')
tbl = doc.add_table(rows=1, cols=3)
tbl.style = 'Light Grid Accent 1'
hdr = tbl.rows[0].cells
for c, t in zip(hdr, ('类型', '含义', '本项目是否实现')):
    c.paragraphs[0].add_run(t).bold = True
rows = [
    ('Endpoint 端点/顶点', '直线两端、弧/矩形/多边形/样条的顶点', '✅'),
    ('Center 圆心', '圆/圆弧的圆心', '✅'),
    ('Intersection 交点', '两图元相交处（线×线、线×圆、圆×圆…）', '✅'),
    ('Tangent 切点', '由已知点向圆/弧作切线的切点', '✅（画直线第二点时）'),
    ('Midpoint 中点', '线段两端坐标平均', '通用做法，本项目未单独实现'),
    ('Perpendicular 垂足', '点到直线的垂足投影', '通用做法，本项目未实现'),
    ('Quadrant 象限点', '圆在坐标轴方向上的 0/90/180/270° 点', '本项目作为圆心辅助点展示'),
]
for a, b, c in rows:
    cells = tbl.add_row().cells
    cells[0].text = a
    cells[1].text = b
    cells[2].text = c
doc.add_paragraph()
add_figure('fig1_type_overview.png',
           '图 1-1  捕捉类型与标记样式总览（左→右：端点、圆心、交点、切点；颜色与形状一一对应 drawForeground 实际渲染）',
           width_cm=15.5)
para('本项目用枚举 SnapType{None, Endpoint, Center, Intersection, Tangent}（src/core/Snap.h:18）表达，'
     '结构 SnapResult 还记录命中点的世界坐标、type、以及 elemA/elemB（用于交点记录两个图元、'
     '切点记录所属圆）。')

# ===================== 二、算法实现 =====================
para('二、计算方法与算法实现', style='Heading 1')
para('三类候选点的算法分别是：')

para('1) 端点 / 圆心 —— snapHandles(const Element&)', style='Heading 2')
para('按图元类型枚举“真实几何点”：直线取两端点；矩形取四角；圆只取圆心（半径手柄是虚构辅助点，'
     '不参与）；弧取圆心 + 起/终点；多边形/样条取全部顶点。复杂度 O(图元数 × 顶点数)，'
     '可直接对全部图元遍历。')

para('2) 交点 —— geo::intersectElements(A, B)', style='Heading 2')
para('两两求交。为避免 O(n²) 全量求交，先做一次宽筛：只有当两个图元到光标的距离都 ≤ tol×6 '
     '（见 Snap.cpp:95，broad = tol*6）时才真正求交。交点本身就是一条/多条线段或点，直接作为候选。')

para('3) 切点 —— tangentPointsFromPointToCircle(P, C, r, out[2])', style='Heading 2')
para('这是画直线第二点“切圆”的关键。公式：设 φ = atan2(P.y−C.y, P.x−C.x)（圆心指向外部点的方向角），'
     '则切点方向角 θ = φ ± α，其中 α = acos(r/d)、d = |P−C|。两个切点即 C + r·(cosθ, sinθ)。')
bullet('d < r（点在圆内）→ 无实切线，返回 0；')
bullet('弧还要过滤：切点必须落在弧段的扫掠角范围内（geo::angleInSweep），否则不算。')
code('int tangentPointsFromPointToCircle(const Vec3& P, const Vec3& C, double r, Vec3 out[2]) {\n'
     '    const double dx = P.x - C.x, dy = P.y - C.y;\n'
     '    const double d = std::hypot(dx, dy);\n'
     '    if (d < r - 1e-9) return 0;                 // 点在圆内：无实切线\n'
     '    const double phi   = std::atan2(dy, dx);\n'
     '    const double alpha = std::acos(std::min(1.0, r / d));\n'
     '    int n = 0;\n'
     '    for (double ang : {phi + alpha, phi - alpha})\n'
     '        out[n++] = Vec3(C.x + r*std::cos(ang), C.y + r*std::sin(ang), 0);\n'
     '    return n;\n'
     '}')

para('4) 综合入口 snap(elems, w, otherEnd, tol)', style='Heading 2')
para('用一个 consider(p, type, a, b) 闭包统一接收候选，内部只在 dist(p,w) < bestD 时才更新最优解。'
     'otherEnd 非空时（即画直线第二点），额外计算“由 otherEnd 向各圆/弧作切线的切点”参与竞争。')
add_figure('fig2_pipeline.png',
           '图 2-1  捕捉点获取管线（鼠标坐标 w → snap::snap → 三类候选源并行 → consider 取最近 → SnapResult）',
           width_cm=15.5)

# ===================== 三、容差策略 =====================
para('三、捕捉范围的设定策略', style='Heading 1')
para('容差必须以屏幕像素为准、与缩放无关，否则放大/缩小时手感会剧烈变化。本项目做法'
     '（src/ui/GraphicsView.h:140）：')
code('double worldTol() const { return PICK_TOL_PX / std::max(1e-6, zoom_); }')
para('即“屏幕 N 像素”换算成当前缩放下的世界坐标半径。例如 PICK_TOL_PX = 10，无论放大到多大，'
     '光标离特征 10 个屏幕像素内就会吸附，体验一致。')
para('两级筛选进一步提高效率与稳定性：', style='Heading 3')
bullet('精细容差 tol = worldTol()：最终判定吸附与否、以及“最近者”的硬门槛（bestD 初始化为 tol）。')
bullet('宽筛 tol×6：仅用于交点求交前的粗排，避免对远处图元做无用求交。')
para('想落自由点，只需把光标移出容差圈即可。像素调大 → 容差圈变大 → 更容易吸附；调小 → 更精确'
     '但更难“咬”住。')

# ===================== 四、优先级 =====================
para('四、多候选时的优先级判断逻辑', style='Heading 1')
para('判定分两步：')
para('1. 距离优先（最近者胜）：对所有满足 d ≤ tol 的候选，取 dist(p, w) 最小者。'
     'consider() 用严格小于更新（if (d < bestD)），所以严格最近者胜出。', style='Heading 3')
para('2. 同距按类型优先级（平局打破）：当距离相等时，按候选被加入的顺序决定——而加入顺序是 '
     'snap() 里写死的：先端点/圆心（snapHandles 遍历），再交点（intersectElements），'
     '最后切点（需 otherEnd）。因此平局时优先级为 端点/圆心 > 交点 > 切点。', style='Heading 3')
para('工程取舍：交点若恰好落在端点上，会被端点“胜出”（退化情形）。本项目未实现“同时切两圆求双切线”，'
     '留作扩展点。')
add_figure('fig3_priority.png',
           '图 4-1  多候选优先级判定（左：距离优先取 dist(p,w) 最小；右：同距时按加入顺序 端点/圆心 > 交点 > 切点 打破平局）',
           width_cm=15.5)

# ===================== 五、触发方式 =====================
para('五、绘制时如何触发捕捉', style='Heading 1')
para('触发是自动且常开的，无需按住任何修饰键。以画直线为例，代码路径非常直白：')
bullet('激活绘制工具（line），drawing_ = true，进入 pendingPts_ 待定点状态（GraphicsView.cpp:1022）。')
bullet('每次 mouseMove，视图用 snapWorld(w) 把当前光标替换成吸附点，并刷新预览线'
       '（GraphicsView.cpp:1203：updatePreview(snapWorld(w))）。')
bullet('snapWorld() 内部（GraphicsView.cpp:414）：若已有待定点，则把最后一个待定点当作 otherEnd '
       '传入 snap::snap()，从而第二点才能算切点；否则只做端点/圆心/交点捕捉。')
bullet('drawForeground()（GraphicsView.cpp:444）在 snapNow_.snapped 为真时，按类型画不同颜色/形状'
       '标记：端点/圆心=方框（蓝/紫）、交点=×（绿）、切点=圆圈（橙），大小约 10px 恒定屏幕像素。')
bullet('单击 handlePress 时直接 pendingPts_.push_back(snapWorld(w))（GraphicsView.cpp:1023）——'
       '拾取点被无缝替换成捕捉点；后续命令（如 finalizeLine）拿到的就是精确吸附坐标。')
para('要得到“自由点”只需把光标移出容差圈；命令行/selftest 也可直接调用 snap::snap() 复用同一套'
     '几何逻辑（这也是把几何层独立出来的好处）。')
add_figure('fig4_trigger.png',
           '图 5-1  绘制时触发捕捉的完整流程（从激活绘制工具到落点替换，再到退出 / 自由点）',
           width_cm=15.5)
add_figure('fig5_linedraw.png',
           '图 5-2  直线绘制捕捉实操示意：第一点（蓝方框·端点）→ 光标靠近圆出现容差圈 → 切点（橙圈）→ 直线切于圆；图中同时给出两交叉线的交点（绿×）与圆的圆心（紫方框）作为对照',
           width_cm=15.5)

# ===================== 代码落点 =====================
para('六、代码落点小结', style='Heading 1')
bullet('类型与算法：src/core/Snap.h / Snap.cpp')
bullet('触发与渲染：src/ui/GraphicsView.cpp（snapWorld 414、drawForeground 444、handlePress 1023、'
       'updatePreview 1203）')
bullet('容差：src/ui/GraphicsView.h:140（worldTol）')

# ===================== 输出 =====================
out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'docs')
os.makedirs(out_dir, exist_ok=True)
out_path = os.path.join(out_dir, '图形绘制系统智能捕捉点功能说明.docx')
doc.save(out_path)
print('saved:', out_path)
