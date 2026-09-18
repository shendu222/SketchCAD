# -*- coding: utf-8 -*-
"""生成《航空CAD-草图绘制模块-深度思考（直线绘制智能捕捉）》聚焦文档。

仅导出本轮「画直线时的智能点位捕捉（端点/圆心/交点/切点）」的设计推演。
"""
from docx import Document
from docx.shared import Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT

OUT = r"D:/01-CAD/02-avticationCAD/docs/航空CAD-草图绘制模块-深度思考（直线绘制智能捕捉）.docx"

DOC_TITLE = "航空CAD 草图绘制模块 — 深度思考（直线绘制智能捕捉）"
SUBTITLE = "C++17 / Qt6 实现 · 端点/圆心/交点/切点捕捉 · 切线几何 · 与画线交互同源"

doc = Document()
normal = doc.styles["Normal"]
normal.font.name = "Microsoft YaHei"
normal.font.size = Pt(10.5)

def H1(t): return doc.add_heading(t, level=1)
def H2(t): return doc.add_heading(t, level=2)
def P(t, bold=False, italic=False):
    p = doc.add_paragraph(); r = p.add_run(t); r.bold = bold; r.italic = italic; return p
def bullet(t):
    p = doc.add_paragraph(style="List Bullet"); p.add_run(t); return p
def code(t):
    p = doc.add_paragraph(); r = p.add_run(t)
    r.font.name = "Consolas"; r.font.size = Pt(9); r.font.color.rgb = RGBColor(0x1F,0x49,0x7D); return p
def table(headers, rows):
    t = doc.add_table(rows=1, cols=len(headers)); t.style = "Light Grid Accent 1"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i,h in enumerate(headers):
        c = t.rows[0].cells[i]; c.text=""; run=c.paragraphs[0].add_run(h); run.bold=True
    for row in rows:
        cells = t.add_row().cells
        for i,v in enumerate(row): cells[i].text=str(v)
    return t

# 封面
title = doc.add_paragraph(); title.alignment = WD_ALIGN_PARAGRAPH.CENTER
tr = title.add_run(DOC_TITLE); tr.bold=True; tr.font.size=Pt(19); tr.font.color.rgb=RGBColor(0x1A,0x1D,0x28)
sub = doc.add_paragraph(); sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
sr = sub.add_run(SUBTITLE); sr.italic=True; sr.font.size=Pt(10.5); sr.font.color.rgb=RGBColor(0x55,0x55,0x55)
doc.add_paragraph()

P("本文档聚焦 sketch-cad-cpp（C++17 / Qt 6.10.1 / MinGW-w64 GCC 13.1.0）本轮新增的「直线绘制智能捕捉」："
  "画直线时，端点可吸附到五种几何特征点——直线/弧的端点、圆/弧的圆心、两图元的交点、以及由已知另一端点向圆作切线的切点。"
  "重点在「几何计算怎么算才准」「与现有画线交互如何咬合」「精度如何被无界面自测钉死」。更庞大的架构基线见综合文档。")

P("验证基线：干净构建 0 警告 0 错误；核心自测 139 项全绿，其中 §19 共 14 项用例专门覆盖本功能。", bold=True)

# 1
H1("一、五种捕捉方式的几何归类")
P("需求列出的五种点，按其几何来源可归为两类计算引擎，避免在 UI 里各写一套：")
table(
    ["捕捉类型", "几何来源", "计算引擎", "依赖 anotherEnd?"],
    [["直线/弧端点", "图元控制点", "snapHandles 取真实点", "否"],
     ["圆/弧圆心", "图元控制点", "snapHandles 取 center", "否"],
     ["两图元交点", "图元求交", "intersectElements（复用）", "否"],
     ["切点（切于圆）", "由外部点作切线", "tangentPointsFromPointToCircle", "是（首端点）"]],
)
P("关键决策：交点直接复用既有的 geo::intersectElements（直线×直线、直线×圆、圆×圆、含圆弧角度过滤都已实现），"
  "不另写求交逻辑——又一次印证「core 层几何下沉」价值：新交互只是换个触发条件，底层零新增代码。")

# 2 切线几何
H1("二、切点几何：由外部点向圆作切线")
P("需求 1 的「直线与选定圆相切，端点为该圆切点」在数学上等价于：已知一个端点 P（已落定的另一端点），"
  "求从 P 向圆 (C, r) 所作切线的切点 T。T 必须满足 |T−C|=r 且 PT ⟂ CT。")
code("int tangentPointsFromPointToCircle(const Vec3& P, const Vec3& C, double r, Vec3 out[2]) {\n"
     "    const double d = hypot(P.x-C.x, P.y-C.y);\n"
     "    if (d < r - 1e-9) return 0;            // 点在圆内：无实切线\n"
     "    const double phi   = atan2(P.y-C.y, P.x-C.x);   // 圆心指向 P 的方向角\n"
     "    const double alpha = acos(min(1.0, r/d));        // 半张角\n"
     "    out[0] = C + r*(cos(phi+alpha), sin(phi+alpha)); // 两个切点\n"
     "    out[1] = C + r*(cos(phi-alpha), sin(phi-alpha));\n"
     "    return 2;\n"
     "}")
H2("2.1 边界与精度")
bullet("点在圆内（d<r）：返回 0——不存在实切线（自测 §19.1 验证）。")
bullet("点在圆上（d≈r）：alpha≈0，两切点重合于 P 投影点，退化为单点，不崩溃。")
bullet("弧的切点必须落在其角度扫掠范围内：对 arc 类型再用 angleInSweep 过滤，圆则恒通过。")
bullet("垂直性由构造保证：T−C 与 P−T 的点积理论为 0；自测 §19.1 断言 |dot|<1e-6 且 |T−C−r|<1e-6，把「在圆上且垂直」钉死。")

# 3 综合捕捉
H1("三、综合捕捉 snap()：最近者胜")
P("snap(elems, w, otherEnd, tol) 在容差 tol 内收集所有候选（端点/圆心、交点、切点），取离光标 w 最近者。"
  "之所以「最近距离优先」而非「按类型排优先级」，是因为它最贴近主流 CAD 的体感，且实现最省：")
bullet("端点/圆心：遍历每个可见图元的 snapHandles，逐个 consider（圆仅 center，弧含 center+起/终，直线两端，矩形四角，多边形/样条全顶点）。")
bullet("交点：先以 distanceToElement(A,w)<tol*6 粗筛靠近光标的图元（O(n²) 但 n 通常很小），再对筛选后的两两对求交并 consider。")
bullet("切点：仅当 otherEnd 非空时，对每个圆/弧算 tangentPointsFromPointToCircle，过滤弧角范围后 consider。")
code("auto consider = [&](const Vec3& p, SnapType tp, int a, int b=-1) {\n"
     "    const double d = p.distTo(w);\n"
     "    if (d < bestD) { bestD = d; best.point = p; best.snapped=true; best.type=tp; best.elemA=a; best.elemB=b; }\n"
     "};")
H2("3.1 已知设计权衡：交点与端点重合时端点胜")
P("若交点恰好与某图元端点重合（如两线在 (0,0) 相接），二者到光标距离相等，consider 顺序上端点先入栈、交点距离相等不更新，"
  "最终返回端点。这是「最近者胜」的天然结果。自测 §19.5 专门用「交点不落在任一端点上」的配置来验证 Intersection 类型，"
  "既覆盖功能又避免与端点捕捉在退化情形下互相干扰。若未来需要『交点恒优先』，只需在 consider 里给 Intersection 一个微小距离折扣即可，改动局部。")

# 4 与画线交互咬合
H1("四、与画线交互的咬合（GraphicsView）")
P("捕捉不是独立功能，而是「拾取点」的前置滤波器：")
bullet("snapWorld(w)：以 pendingPts_.back() 作为 otherEnd 调用 snap；返回的 point 替换原始光标坐标。首点时 pendingPts_ 为空 → otherEnd=null → 仅做端点/圆心/交点捕捉（无切线）。")
bullet("handlePress：画线分支把 snapWorld(w) 推入 pendingPts_，于是落定的端点已是吸附点，后续镜像/联动/标注都吃到正确坐标。")
bullet("mouseMove：画线预览改为 updatePreview(snapWorld(w))，预览线段的游离端实时吸附，所见即所得。")
bullet("drawForeground：按 snapNow_.type 画不同颜色/形状的标记（端点/圆心=方框，交点=×，切点=圆圈），约 10px 恒定屏幕大小，帮助用户确认吸附类型。")
bullet("resetPending / Esc：清空 snapNow_，状态机不残留。")
bullet("容差复用 worldTol() = PICK_TOL_PX / zoom_，与端点拖拽捕捉、命中检测同一把尺，避免「能抓端点却吸不上交点」的割裂。")

# 5 自测呼应
H1("五、精度如何被自测钉死（§19，14 项）")
bullet("切线：外部点→圆返回 2 切点；每点 |T−C−r|<1e-6 且在圆上；PT·CT<1e-6（垂直）；圆内点返回 0。")
bullet("圆心：光标贴 (100,100) r=40 圆心 → Center 且坐标精确为 (100,100)。")
bullet("直线端点/弧端点：贴 (0,0) 与弧起/终点 → Endpoint 且坐标精确；弧端点不含圆心（snapHandles 对 arc 给 center+起+终，但端点判定 i!=0）。")
bullet("交点：两轴交 (0,0) → Intersection，且 elemA/elemB 记录两条线 id，供后续自动加「相交」约束可扩展。")
bullet("切点：给定首端点 P=(40,0)、圆 (0,0) r=25，光标近切点 → Tangent 且精确落在圆上。")
bullet("自由点：远离任何特征 → snapped=false，point==w，不污染自由绘制。")
P("这些断言全部基于几何解析值（圆心、半径、切线公式），不依赖浮点拟合，因此一旦有人把容差或过滤写错，测试立刻变红。")

# 6 留白
H1("六、刻意留白（非本轮范围）")
bullet("「同时切两圆」的切线：本实现只允许第二点切于一个圆（另一端点已知）。若需画一条同时外切两圆的公切线，需要 『选圆A→选圆B』的双圆切线求解，留待后续。")
bullet("捕捉命中后自动加约束（如「端点重合」「相切」）：snap 已记录 elemA/elemB 与类型，未来可据其自动建约束，与需求3的拓扑保持呼应。")
bullet("三维/非笛卡尔平面下的捕捉：当前 model 仅标准平面，snap 直接吃世界坐标；若引入多工作平面，需先把 w 投影到目标平面再算。")

# 7
H1("七、验证结论")
bullet("干净构建（CMake + Ninja + MinGW GCC 13.1.0）：0 警告 / 0 错误。")
bullet("核心自测 SketchSelfTest：139 项通过 / 0 失败。")
bullet("新增文件：src/core/Snap.h / src/core/Snap.cpp（已注册进 SketchCAD 与 SketchSelfTest 两目标）；"
      "改动：src/ui/GraphicsView.cpp/.h（snapWorld + 标记绘制）、tests/selftest.cpp（§19）。")
bullet("生成脚本：sketch-cad-cpp/tools/gen_deepthink_snap.py（可重跑复现本文档）。")

doc.add_paragraph()
end = doc.add_paragraph()
er = end.add_run("—— 聚焦文档由深度思考过程自动导出（sketch-cad-cpp / C++17 / Qt6）")
er.italic=True; er.font.size=Pt(8.5); er.font.color.rgb=RGBColor(0x88,0x88,0x88)
doc.save(OUT)
print("SAVED:", OUT)
