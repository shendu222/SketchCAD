# -*- coding: utf-8 -*-
"""生成《图形绘制系统智能捕捉点功能说明》所需的 5 张示意图 / 流程图（PNG）。
全部以 matplotlib(Agg) 渲染，使用系统中文字体，白底深字以匹配 Word 文档的浅色主题。
输出目录：docs/figures/
"""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Rectangle, Circle, FancyBboxPatch, Ellipse
from matplotlib.lines import Line2D

plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "SimSun", "DejaVu Sans"]
plt.rcParams["axes.unicode_minus"] = False
plt.rcParams["font.size"] = 11

FIG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "figures")
os.makedirs(FIG_DIR, exist_ok=True)

# 配色（浅色主题）
C_BG = "white"
C_BOX = "#EAF1FA"
C_BOX_B = "#2B6CB0"
C_BOX2 = "#EAF7EE"
C_BOX2_B = "#2F855A"
C_BOX3 = "#FBF0E6"
C_BOX3_B = "#C05621"
C_TXT = "#1A202C"
C_ARROW = "#4A5568"
C_BLUE = "#2B6CB0"   # 端点 / 圆心方框
C_PURPLE = "#6B46C1" # 圆心
C_GREEN = "#2F855A"  # 交点 X
C_ORANGE = "#C05621" # 切点圈
C_GRAY = "#A0AEC0"


def _arrow(ax, x1, y1, x2, y2, color=C_ARROW, style="-|>", rad=0.0, lw=1.6, label=None):
    a = FancyArrowPatch((x1, y1), (x2, y2),
                        arrowstyle=style, mutation_scale=14,
                        connectionstyle=f"arc3,rad={rad}",
                        color=color, lw=lw, shrinkA=2, shrinkB=2)
    ax.add_patch(a)
    if label:
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        ax.text(mx, my + 0.12, label, fontsize=9, color=C_ARROW, ha="center", va="bottom")


def _box(ax, x, y, w, h, text, fc=C_BOX, ec=C_BOX_B, tc=C_TXT, fs=11, bold=True):
    b = FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                       boxstyle="round,pad=0.02,rounding_size=0.08",
                       fc=fc, ec=ec, lw=1.6)
    ax.add_patch(b)
    ax.text(x, y, text, fontsize=fs, color=tc, ha="center", va="center",
            weight="bold" if bold else "normal", wrap=True)


def marker(ax, x, y, kind, s=0.10):
    """在坐标 (x,y) 画一个捕捉标记。kind: end/center/inter/tan"""
    if kind == "end":
        ax.add_patch(Rectangle((x - s, y - s), 2 * s, 2 * s, fc="none", ec=C_BLUE, lw=2.2))
    elif kind == "center":
        ax.add_patch(Rectangle((x - s, y - s), 2 * s, 2 * s, fc="none", ec=C_PURPLE, lw=2.2))
    elif kind == "inter":
        d = s * 1.25
        ax.add_line(Line2D([x - d, x + d], [y - d, y + d], color=C_GREEN, lw=2.4))
        ax.add_line(Line2D([x - d, x + d], [y + d, y - d], color=C_GREEN, lw=2.4))
    elif kind == "tan":
        ax.add_patch(Circle((x, y), s, fc="none", ec=C_ORANGE, lw=2.4))


# ============================================================
# 图1：捕捉类型与标记样式总览（四张卡片）
# ============================================================
def fig_overview(path):
    fig, axes = plt.subplots(1, 4, figsize=(11.2, 3.4))
    fig.patch.set_facecolor(C_BG)
    titles = ["端点 Endpoint", "圆心 Center", "交点 Intersection", "切点 Tangent"]
    subs = ["直线/弧/矩形/多边形/样条端点", "圆/圆弧的圆心", "两图元相交处", "向圆作切线的切点"]
    for ax, t, s in zip(axes, titles, subs):
        ax.set_xlim(-1.3, 1.3); ax.set_ylim(-1.1, 1.1)
        ax.set_aspect("equal"); ax.axis("off")
        ax.set_title(t, fontsize=12, color=C_TXT, weight="bold", pad=8)
        # 小示意图
        if t.startswith("端点"):
            ax.add_line(Line2D([-0.9, 0.9], [-0.3, 0.4], color="#444", lw=2))
            marker(ax, -0.9, -0.3, "end")
            marker(ax, 0.9, 0.4, "end")
        elif t.startswith("圆心"):
            ax.add_patch(Circle((0, 0), 0.8, fc="none", ec="#444", lw=2))
            marker(ax, 0, 0, "center")
        elif t.startswith("交点"):
            ax.add_line(Line2D([-0.9, 0.9], [-0.6, 0.6], color="#444", lw=2))
            ax.add_line(Line2D([-0.5, 0.7], [0.8, -0.7], color="#444", lw=2))
            marker(ax, 0.0, 0.0, "inter")
        else:  # 切点
            ax.add_patch(Circle((0.2, 0), 0.75, fc="none", ec="#444", lw=2))
            # 从左侧外部点作切线
            ax.add_line(Line2D([-1.0, -0.05], [-0.9, -0.55], color="#444", lw=2))
            marker(ax, -0.05, -0.55, "tan")
        ax.text(0, -1.0, s, fontsize=8.5, color="#666", ha="center", va="top")
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(path, dpi=150, facecolor=C_BG)
    plt.close(fig)


# ============================================================
# 图2：捕捉点获取管线（流程图）
# ============================================================
def fig_pipeline(path):
    fig, ax = plt.subplots(figsize=(9.6, 6.0))
    fig.patch.set_facecolor(C_BG)
    ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.axis("off")

    # 起点
    _box(ax, 5, 9.2, 3.4, 0.8, "鼠标光标世界坐标 w\n（含缩放、屏幕像素容差圈 worldTol）", fs=10)
    _box(ax, 5, 7.8, 4.2, 0.9, "snap::snap(elems, w, otherEnd, tol)\n综合捕捉入口（纯几何层）", fc="#EDE9FE", ec="#6B46C1", fs=10)

    _arrow(ax, 5, 8.55, 5, 7.35)

    # 三条并行分支
    branches = [
        (2.0, "snapHandles(const Element&)", "枚举真实几何点\n→ 端点 / 圆心候选", C_BOX, C_BOX_B),
        (5.0, "intersectElements(A, B)\n宽筛 tol×6", "两两求交\n→ 交点候选", C_BOX2, C_BOX2_B),
        (8.0, "tangentPointsFromPointToCircle\n需 otherEnd（画直线第二点）", "φ±α 公式\n→ 切点候选", C_BOX3, C_BOX3_B),
    ]
    for x, t1, t2, fc, ec in branches:
        _box(ax, x, 5.6, 2.7, 1.0, t1, fc=fc, ec=ec, fs=9)
        _box(ax, x, 4.1, 2.7, 0.9, t2, fc=fc, ec=ec, fs=8.5)
        _arrow(ax, 5, 7.35, x, 6.1)

    # 汇合
    _box(ax, 5, 2.7, 4.4, 0.95, "consider(p,type,a,b)：dist(p,w) < bestD 才更新\n严格最近者胜出", fs=9.5)
    for x, *_ in branches:
        _arrow(ax, x, 3.65, 5, 3.2)
    _arrow(ax, 5, 3.2, 5, 2.3)

    _box(ax, 5, 1.2, 5.0, 1.0, "SnapResult{ point, type, elemA, elemB, snapped }\n视图层据此替换拾取点并绘制标记", fc="#EAF1FA", ec=C_BOX_B, fs=10)
    _arrow(ax, 5, 2.25, 5, 1.75)

    fig.tight_layout()
    fig.savefig(path, dpi=150, facecolor=C_BG)
    plt.close(fig)


# ============================================================
# 图3：多候选优先级判定
# ============================================================
def fig_priority(path):
    fig, ax = plt.subplots(figsize=(9.6, 5.2))
    fig.patch.set_facecolor(C_BG)
    ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.axis("off")

    # 左：距离优先
    ax.text(2.6, 9.2, "① 距离优先（最近者胜）", fontsize=12, weight="bold", color=C_TXT, ha="center")
    ax.add_patch(Circle((2.6, 5.5), 0.28, fc="#CBD5E0", ec="#4A5568", lw=1.5))
    ax.text(2.6, 5.5, "w", fontsize=10, ha="center", va="center", weight="bold")
    cand = [(0.7, 7.0, C_GRAY, "d1"), (2.6, 7.6, C_GREEN, "d2(最近)"), (4.5, 6.8, C_GRAY, "d3")]
    for x, y, c, lab in cand:
        _arrow(ax, 2.6, 5.5, x, y, color=c, style="-", lw=1.4)
        ax.plot([x], [y], "o", color=c, ms=8)
        ax.text(x, y + 0.25, lab, fontsize=8.5, color=c, ha="center")
    ax.text(2.6, 4.4, "取 dist(p,w) 最小者\nconsider(): if (d < bestD)", fontsize=9, color="#444",
            ha="center", va="top")

    # 右：类型平局打破（阶梯）
    ax.text(7.4, 9.2, "② 同距平局打破（类型加入顺序）", fontsize=12, weight="bold", color=C_TXT, ha="center")
    ladder = [
        (7.4, 7.6, "端点 / 圆心", C_BLUE),
        (7.4, 6.2, "交点", C_GREEN),
        (7.4, 4.8, "切点", C_ORANGE),
    ]
    for i, (x, y, t, c) in enumerate(ladder):
        _box(ax, x, y, 2.8, 0.9, t, fc="white", ec=c, fs=10.5)
        if i < len(ladder) - 1:
            _arrow(ax, x, y - 0.45, x, ladder[i + 1][1] + 0.45, color=c)
    ax.text(7.4, 3.6, "优先级：端点/圆心 > 交点 > 切点\n（交点恰落端点上 → 端点胜，退化情形）",
            fontsize=9, color="#444", ha="center", va="top")

    # 中间分隔
    ax.add_line(Line2D([5.0, 5.0], [3.5, 9.0], color="#CBD5E0", lw=1.5, ls="--"))

    fig.tight_layout()
    fig.savefig(path, dpi=150, facecolor=C_BG)
    plt.close(fig)


# ============================================================
# 图4：绘制时触发捕捉流程
# ============================================================
def fig_trigger(path):
    fig, ax = plt.subplots(figsize=(9.6, 7.4))
    fig.patch.set_facecolor(C_BG)
    ax.set_xlim(0, 10); ax.set_ylim(0, 12); ax.axis("off")

    steps = [
        ("① 激活绘制工具 line", "drawing_ = true，进入 pendingPts_ 待定点状态", C_BOX_B),
        ("② 每次 mouseMove", "updatePreview(snapWorld(w)) 刷新预览线", C_BOX_B),
        ("③ snapWorld() 内部", "有 otherEnd（已落第一点）→ 含『切点』候选；否则仅端点/圆心/交点", C_BOX3_B),
        ("④ drawForeground 绘制标记", "端点/圆心=方框(蓝/紫)，交点=×(绿)，切点=圆圈(橙)，约10px", C_BOX2_B),
        ("⑤ 单击 handlePress", "pendingPts_.push_back(snapWorld(w))，拾取点被替换为精确吸附坐标", C_BOX_B),
        ("⑥ 退出 / 自由点", "移出容差圈=自由点；Esc / 右键=取消并退出绘制", "#718096"),
    ]
    ys = [11.0, 9.4, 7.8, 6.2, 4.6, 3.0]
    for (t, d, c), y in zip(steps, ys):
        _box(ax, 5, y, 8.4, 1.1, t + "\n" + d, fc="#F7FAFC", ec=c, fs=10, bold=False)
    for y1, y2 in zip(ys[:-1], ys[1:]):
        _arrow(ax, 5, y1 - 0.55, 5, y2 + 0.55, color=C_ARROW)

    fig.tight_layout()
    fig.savefig(path, dpi=150, facecolor=C_BG)
    plt.close(fig)


# ============================================================
# 图5：直线绘制触发捕捉示意（实操画布）
# ============================================================
def fig_linedraw(path):
    fig, ax = plt.subplots(figsize=(9.6, 6.2))
    fig.patch.set_facecolor(C_BG)
    ax.set_xlim(-6, 6); ax.set_ylim(-3.2, 3.2); ax.set_aspect("equal")
    ax.axhline(0, color="#E2E8F0", lw=1)
    ax.axvline(0, color="#E2E8F0", lw=1)
    ax.axis("off")

    # 圆（可切）
    cc = (1.6, 0.4); r = 1.4
    ax.add_patch(Circle(cc, r, fc="none", ec="#444", lw=2))
    # 第一条线：从 (-5,-1.2) 到圆上第一点（蓝方框端点）
    p1 = (-5.0, -1.2)
    # 切点示意（约在圆右下）
    tan_pt = (cc[0] + r * 0.94, cc[1] - r * 0.34)
    ax.add_line(Line2D([p1[0], tan_pt[0]], [p1[1], tan_pt[1]], color="#2B6CB0", lw=2))
    marker(ax, p1[0], p1[1], "end")
    marker(ax, tan_pt[0], tan_pt[1], "tan")

    # 容差圈（光标附近）
    cursor = (3.2, 1.9)
    ax.add_patch(Circle(cursor, 0.55, fc="none", ec=C_GRAY, lw=1.4, ls="--"))
    ax.plot([cursor[0]], [cursor[1]], "o", color=C_GRAY, ms=5)

    # 两交叉线（交点 X）
    ax.add_line(Line2D([-4.5, -1.0], [2.2, -1.8], color="#718096", lw=1.8))
    ax.add_line(Line2D([-2.5, -0.5], [-2.0, 1.6], color="#718096", lw=1.8))
    inter = (-1.6, -0.4)
    marker(ax, inter[0], inter[1], "inter")

    # 圆心标记
    marker(ax, cc[0], cc[1], "center")

    # 标注
    ax.text(p1[0] - 0.1, p1[1] - 0.5, "第一点(端点·蓝方框)", fontsize=8.5, color=C_BLUE, ha="center")
    ax.text(tan_pt[0] + 0.2, tan_pt[1] - 0.6, "切点(橙圈)\n直线切于圆", fontsize=8.5, color=C_ORANGE, ha="left")
    ax.text(cursor[0] + 0.7, cursor[1], "容差圈\n(约10px)", fontsize=8.5, color="#718096", ha="left", va="center")
    ax.text(inter[0] - 0.2, inter[1] + 0.5, "交点(绿×)", fontsize=8.5, color=C_GREEN, ha="center")
    ax.text(cc[0] + r + 0.1, cc[1] + 0.5, "圆心(紫方框)", fontsize=8.5, color=C_PURPLE, ha="left")

    fig.tight_layout()
    fig.savefig(path, dpi=150, facecolor=C_BG)
    plt.close(fig)


def main():
    paths = {
        "overview": os.path.join(FIG_DIR, "fig1_type_overview.png"),
        "pipeline": os.path.join(FIG_DIR, "fig2_pipeline.png"),
        "priority": os.path.join(FIG_DIR, "fig3_priority.png"),
        "trigger": os.path.join(FIG_DIR, "fig4_trigger.png"),
        "linedraw": os.path.join(FIG_DIR, "fig5_linedraw.png"),
    }
    fig_overview(paths["overview"])
    fig_pipeline(paths["pipeline"])
    fig_priority(paths["priority"])
    fig_trigger(paths["trigger"])
    fig_linedraw(paths["linedraw"])
    for k, p in paths.items():
        print("wrote", k, os.path.getsize(p), "bytes")


if __name__ == "__main__":
    main()
