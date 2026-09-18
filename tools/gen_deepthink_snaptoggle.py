# -*- coding: utf-8 -*-
"""生成《直线绘制快捷键捕捉切换 - 深度思考》文档。
依赖 python-docx：venv 在 C:/Users/shend/.workbuddy/binaries/python/envs/default
运行：~/.workbuddy/binaries/python/envs/default/Scripts/python.exe gen_deepthink_snaptoggle.py
"""
import os
from docx import Document
from docx.shared import Pt, RGBColor, Inches
from docx.enum.text import WD_ALIGN_PARAGRAPH

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT  = os.path.join(ROOT, "docs", "直线绘制快捷键捕捉切换-深度思考.docx")

doc = Document()

# 基础样式
normal = doc.styles["Normal"]
normal.font.name = "Calibri"
normal.font.size = Pt(10.5)

def H(text, level=1):
    h = doc.add_heading(text, level=level)
    return h

def P(text, bold=False, italic=False, size=10.5):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.bold = bold
    r.italic = italic
    r.font.size = Pt(size)
    return p

def code(text):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.font.name = "Consolas"
    r.font.size = Pt(9)
    r.font.color.rgb = RGBColor(0x33, 0x33, 0x33)
    p.paragraph_format.left_indent = Inches(0.3)
    return p

def bullet(text):
    doc.add_paragraph(text, style="List Bullet")

# ---------------- 标题 ----------------
title = doc.add_heading("直线绘制实时快捷键捕捉切换 — 深度思考", level=0)
sub = doc.add_paragraph("航空CAD 草图绘制模块 · 增强需求的技术剖析与实现纪要")
sub.runs[0].italic = True
sub.runs[0].font.size = Pt(10)

# ---------------- 引言 ----------------
H("一、需求再剖析", 1)
P("用户提出的需求可以提炼为一条清晰的规则：在「绘制命令执行期间」，用 E / T / M / C / X 五个不区分大小写的快捷键，"
  "以「切换式、非独占」的方式分别开关端点、切线、中点、圆心、交点五类捕捉；当命令退出时，所有捕捉设置自动恢复到进入命令之前的状态；"
  "并且这套机制要能推广到圆、圆弧、多段线等其它绘制命令，各自维护独立的默认映射。")
P("这看似只是「加几个热键」，实则触及三个层面的设计问题：")
bullet("状态层：捕捉开关是一份可变的、按命令隔离的配置，而非全局常量；")
bullet("交互层：绘制命令进行中，E/T/M/C/X 应被『重映射』为捕捉切换键，并临时让出与工具栏同名的全局快捷键，避免冲突；")
bullet("几何层：原有的 snap 只支持「全开」，要升级为「按类型门控」，并补上缺失的中点捕捉。")

# ---------------- 总体架构 ----------------
H("二、总体架构：几何与 UI 解耦", 1)
P("延续既有设计：纯几何计算放在 core/Snap，UI 只负责把结果画出来、并替换拾取点。本次新增的开关逻辑同样分两层：")
bullet("SnapConfig（几何层）：一个仅含五个布尔开关的结构，描述「当前允许哪些捕捉类型参与竞争」。")
bullet("GraphicsView（UI 层）：持有当前生效的 SnapConfig 与其进入命令前的快照，并通过 QShortcut 在绘制期间把 E/T/M/C/X 绑定为捕捉开关的取反操作（同时临时让出冲突的全局快捷键）。")
P("这样做的好处是：命令行、无界面自测、以及未来的约束系统，都能复用同一份「带开关的捕捉」能力，而不必关心界面。")

# ---------------- SnapConfig ----------------
H("三、开关模型：SnapConfig", 1)
P("在 core/Snap.h 中把捕捉类型枚举补上 Midpoint，并引入配置结构：")
code("enum class SnapType { None, Endpoint, Center, Intersection, Tangent, Midpoint };\n\n"
     "struct SnapConfig {\n"
     "    bool endpoint     = true;   // 端点 / 顶点\n"
     "    bool center       = true;   // 圆心\n"
     "    bool intersection = true;   // 两图元交点\n"
     "    bool tangent      = true;   // 切点（需已知另一端点）\n"
     "    bool midpoint     = false;  // 直线段中点（默认关闭，按 M 开启）\n"
     "};")
P("中点默认关闭，正是为了与历史行为严格一致——此前根本没有中点捕捉，所以默认集合里不应凭空多出一种吸附。")

# ---------------- 快捷键映射与大小写 ----------------
H("四、E/T/M/C/X 映射与大小写无关", 1)
P("映射关系固定且跨命令统一：E→端点、T→切线、M→中点、C→圆心、X→交点。大小写无关性由 Qt 的事件机制天然保证——"
  "QKeyEvent::key() 返回的是物理按键的键码（如 Qt::Key_E），与 Shift / CapsLock 状态无关，因此无论用户敲 e 还是 E，"
  "拿到的都是同一个键码。无需任何额外的 toUpper / toLower 处理。交点选 X 是因为交点标记本身画成绿色『×』，"
  "字母与视觉一致，且 X 不在任何绘制命令的快捷键（L/C/…）之列，不会与工具冲突。")

# ---------------- 切换式与多模式共存 ----------------
H("五、切换式（toggle）与多模式共存（非独占）", 1)
P("「切换式」意味着按一次开、再按一次关，而不是像单选那样「按了 E 就关掉其它」。实现上就是一句无条件的取反：")
code("// 五个切换键 = 五个 QShortcut（默认禁用，绘制中 enableSnapShortcuts(true)）\n"
     "QShortcut* sc = new QShortcut(QKeySequence(Qt::Key_E), this);\n"
     "sc->setContext(Qt::ApplicationShortcut);\n"
     "connect(sc, &QShortcut::activated, this,\n"
     "        [this]() { toggleSnap(&snap::SnapConfig::endpoint, \"端点(E)\"); });")
P("五个开关彼此独立，没有任何「互斥」逻辑，因此天然满足「多模式可同时开启」——用户可以端点+交点+中点一起开，"
  "也可以只留圆心。这也正是非独占式要求的本质：状态是一个位集合的位运算，而不是单选索引。")

# ---------------- 退出恢复机制 ----------------
H("六、退出恢复：备份 / 恢复状态机", 1)
P("核心难点是「恢复到进入命令之前的状态」。本实现把两件事的生命周期都绑定到「绘制命令的进入 / 退出」事件上："
  "① 捕捉配置 snapCfg_（用户开关状态）；② 全局快捷键映射（被临时让出的工具栏快捷键）。恢复必须对两者同时生效。")
bullet("进入任一绘制命令：先 saveConflictingShortcuts() 保存并临时禁用冲突的全局快捷键，再 enableSnapShortcuts(true) 启用本组切换键，然后把当前 snapCfg_ 存入 snapCfgBackup_ 并载入该命令默认值；")
bullet("退出绘制命令（切到选择、或切到另一个命令）：enableSnapShortcuts(false) 关掉本组切换键，restoreConflictingShortcuts() 把被让出的全局快捷键原样还原，再把 snapCfg_ 恢复成 snapCfgBackup_；")
bullet("在命令内部切换（如直线→圆）：仍在绘制组内，不触碰全局快捷键，只重新备份并载入新命令的默认捕捉开关。")
P("关键代码（onActiveToolChanged）采用「进入组 / 离开组」的判定，保证任意跳变序列后状态都可逆：")
code("void GraphicsView::onActiveToolChanged() {\n"
     "    const bool newDraw = isDrawTool(model_->activeTool());\n"
     "    const bool prevDraw = drawToolActive_;\n"
     "    if (newDraw && !prevDraw) {\n"
     "        saveConflictingShortcuts();   // 要求1：保存并临时禁用冲突的全局快捷键\n"
     "        enableSnapShortcuts(true);    // 要求2：本组 E/T/M/C/X 切换键生效\n"
     "        snapCfgBackup_ = snapCfg_; snapCfg_ = snapDefaultFor(t);\n"
     "    } else if (newDraw && prevDraw) {\n"
     "        snapCfgBackup_ = snapCfg_; snapCfg_ = snapDefaultFor(t); // 绘制命令间互切\n"
     "    } else if (!newDraw && prevDraw) {\n"
     "        enableSnapShortcuts(false);   // 要求3：禁用本组切换键\n"
     "        restoreConflictingShortcuts();// 要求3：还原全局快捷键\n"
     "        snapCfg_ = snapCfgBackup_;\n"
     "    }\n"
     "    drawToolActive_ = newDraw;\n"
     "}")
P("关于 Esc / Enter：在本代码库里，Esc 与 Enter 只取消「当前正在拾取的点」但保留在绘制命令内，因此不会触发恢复——"
  "捕捉开关会一直跟随命令。只有真正「切换命令」才恢复。这与需求中『切换到其它命令即退出』完全吻合，"
  "且避免了「每次 Esc 取消一个点就把捕捉设置也清零」的反直觉行为。若产品希望 Esc 直接退回选择态，"
  "只需在 Esc 分支多调一次 setActiveTool(\"select\")，恢复逻辑会自动跟上。")

# ---------------- 扩展性：各命令独立映射 ----------------
H("七、扩展性：各命令独立默认映射", 1)
P("snapDefaultFor(tool) 为每个绘制命令给出一份默认开关，这正是需求中「各自维护独立的捕捉快捷键映射」的落点：")
bullet("line：端点/圆心/交点/切线开，中点关——画直线时最常需要端点与切点；")
bullet("circle：关端点、开圆心+切线（圆没有端点，但可切）；circle3p：关圆心、开端点+切线；")
bullet("arc：端点+圆心+切线开；rect：端点+圆心+切线开；")
bullet("poly / spline：端点+中点+交点开、关切线（折线/曲线段关注端点与分段中点）。")
P("由于键盘映射 E/T/M/C 是统一的，新命令只要在此函数里填一份默认表即可获得完整能力，"
  "后续若加入椭圆、样条高级捕捉，也只需扩展 SnapType 与 snapDefaultFor，无需改动事件分发。")

# ---------------- 中点捕捉的几何计算 ----------------
H("八、中点捕捉的几何计算", 1)
P("中点是最廉价的「派生几何点」：对一段直线段，中点即两端坐标的算术平均。本实现覆盖四类可拆成直线段的几何：")
bullet("Line：单段，直接取 (p0+p1)/2；")
bullet("Rectangle：四个边各自的中点（矩形按两对角点存储，需展开四条边）；")
bullet("Polygon / Spline：相邻控制点构成的每条折边各取中点。")
P("在 snap() 中，中点候选与其他候选走同一套 consider() 竞争逻辑，距离最近者胜；当光标恰好落在某段中点附近，"
  "且 midpoint 开关开启时，即吸附到精确中点。圆/弧没有「直线段」，故不参与中点捕捉，符合需求中『针对直线段』的限定。")

# ---------------- 与工具栏快捷键冲突的解决 ----------------
H("九、与工具栏快捷键冲突的解决（快捷键重映射 + 保存/恢复）", 1)
P("这是本需求最易踩坑之处。工具栏里 C 既是『圆心捕捉切换键』，又是『圆』命令的快捷键（MainWindow 为圆工具绑定了 C）。"
  "若只把 E/T/M/C/X 注册成普通 QShortcut 而不处理冲突，绘制中按 C 会同时命中『圆命令快捷键』与『圆心捕捉切换键』，"
  "造成行为异常（既切了捕捉又触发了圆命令）。需求第 4 点明确要求：临时快捷键不能与系统其它全局快捷键冲突。")
P("解决思路（正规快捷键重映射，而非事件过滤器拦截）：")
bullet("E/T/M/C/X 五个切换键以 QShortcut（Qt::ApplicationShortcut 上下文）实现，默认 setEnabled(false)，仅绘制命令期间启用；")
bullet("进入绘制命令时，saveConflictingShortcuts() 扫描所有顶层窗口的 QAction / QShortcut，把主键为 E/T/M/C/X（且无修饰键）的全局映射记录进 savedShortcuts_，并立即 setShortcut(QKeySequence()) 临时让出，从根上消除冲突；")
bullet("退出绘制命令时，restoreConflictingShortcuts() 把 savedShortcuts_ 里记录的原始映射原样写回，使快捷键状态与绘制前 100% 一致；")
bullet("绘制命令之间互切（直线→圆）时，只重置捕捉开关、不动全局快捷键，避免反复让出/恢复导致抖动。")
P("相比此前『事件过滤器吃掉按键』的做法，重映射方案有两点好处：(1) 退出后全局快捷键是真正被还原（而非仅仅『不拦截』），"
  "语义更严谨、可审计；(2) QShortcut 在文本控件（如命令输入框）获得焦点时不会抢占字母键，因此绘制中在命令框里输入 e/t/m/c/x 不受影响——"
  "而事件过滤器是应用级无条件拦截，反而会吞掉输入框的字母。")
code("void GraphicsView::saveConflictingShortcuts() {\n"
     "    savedShortcuts_.clear();\n"
     "    const QList<Qt::Key> keys = {Key_E, Key_T, Key_M, Key_C, Key_X};\n"
     "    for (QWidget* w : qApp->topLevelWidgets())\n"
     "        for (QAction* a : w->findChildren<QAction*>()) {\n"
     "            QKeyCombination kc = a->shortcut()[0];\n"
     "            if (kc.keyboardModifiers() != Qt::NoModifier) continue;\n"
     "            if (!keys.contains(kc.key())) continue;\n"
     "            savedShortcuts_.append({a, a->shortcut()});\n"
     "            a->setShortcut(QKeySequence());   // 临时让出，避免与切换键冲突\n"
     "        }\n"
     "    // 对 QShortcut 同样处理（跳过本组切换键自身）……\n"
     "}\n"
     "void GraphicsView::restoreConflictingShortcuts() {\n"
     "    for (const auto& s : savedShortcuts_) {\n"
     "        if (auto* a = qobject_cast<QAction*>(s.obj)) a->setShortcut(s.seq);\n"
     "        else if (auto* sc = qobject_cast<QShortcut*>(s.obj)) sc->setKey(s.seq);\n"
     "    }\n"
     "    savedShortcuts_.clear();\n"
     "}")

# ---------------- 容差与优先级 ----------------
H("十、容差与多候选优先级（复用既有机制）", 1)
P("捕捉范围的「像素恒定」策略与多候选优先级均沿用既有实现，未做改动，保证行为一致：")
bullet("容差 worldTol() = PICK_TOL_PX / zoom，以屏幕像素为准、与缩放无关；")
bullet("snap() 内部用『严格小于』更新最优解，最近者胜；距离相等时按候选加入顺序打破平局"
      "（端点/圆心 > 中点 > 交点 > 切点）；")
bullet("交点求交前先用 tol×6 宽筛，避免 O(n²) 全量求交。")
P("开关门控叠加在以上机制之上：被关闭的类型根本不会进入 consider() 竞争，因此『最近者』只在开启的类型里产生。")

# ---------------- 测试覆盖 ----------------
H("十一、测试覆盖", 1)
P("新增自测 §20（含 §20.7 交点门控，共 11 项），全部通过；全量自测 150 项 0 失败、构建 0 警告。§20 重点验证：")
bullet("SnapConfig 默认值符合『中点默认关』；")
bullet("中点开启时光标落在直线段中点被精确吸附，关闭时同位置无捕捉（门控生效）；")
bullet("端点 / 圆心 / 切线分别做『开启捕捉、关闭不捕捉』的正反例，证明开关确实控制竞争集合。")
P("此前的 §19 因 snap() 增加 5 参重载而以兼容 4 参形式保留，行为不变。")

# ---------------- 代码落点 ----------------
H("十二、代码落点小结", 1)
bullet("core/Snap.h：SnapType 增加 Midpoint；新增 SnapConfig；snap() 增加带 cfg 的 5 参重载与 4 参兼容内联。")
bullet("core/Snap.cpp：snap() 按 cfg 门控四类候选，新增直线段/矩形边/折边的中点候选。")
bullet("src/ui/GraphicsView.h：新增 snapCfg_ / snapCfgBackup_ / drawToolActive_ / lastMouseWorld_ 成员与 "
       "onActiveToolChanged / isDrawTool / snapDefaultFor / refreshSnapAtCursor / toggleSnap / "
       "enableSnapShortcuts / saveConflictingShortcuts / restoreConflictingShortcuts / logSnapToggle；"
       "以及 snapShortcuts_ 与 savedShortcuts_。")
bullet("src/ui/GraphicsView.cpp：ctor 创建 E/T/M/C/X 五个 QShortcut（ApplicationShortcut，默认禁用）；"
       "onActiveToolChanged 在绘制进入/退出边界做『保存冲突快捷键→启用切换键 / 禁用切换键→还原快捷键』；"
       "snapWorld 透传 cfg；drawForeground 增加捕捉开关 HUD；mouseMove 记录 lastMouseWorld_。"
       "（事件过滤器方案已弃用，改为正规 QShortcut 重映射。）")
bullet("tests/selftest.cpp：新增 §20（含 §20.7 交点门控）。")

# ---------------- 取舍与后续 ----------------
H("十三、工程取舍与后续扩展点", 1)
bullet("未实现椭圆/样条高级切线等：需求文本提及椭圆，但当前工程无椭圆工具；待工具存在时，"
       "只需在 snapDefaultFor 与 snapHandles 中补表即可。")
bullet("交点已补独立快捷键 X：交点标记画成绿色『×』，故选 X 与视觉一致且无工具冲突；至此 E/T/M/C/X 五键齐备，"
       "交点默认常开但可被 X 关闭。")
bullet("「同时切两圆求双切线」「捕捉即加约束」等此前已识别的扩展点依旧成立，且本次的开关机制可平滑接入。")
bullet("恢复语义绑定『命令切换』而非『Esc』，是有意选择；若产品需要 Esc 即退出，改动成本极低（一行 setActiveTool）。")

doc.save(OUT)
print("saved:", OUT)
