#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《右键拖拽平移-深度思考》docx。

排查「右键拖动时界面元素不跟随、无响应」现象。本文档记录完整的思考链条：
  第一轮：按用户给出的 4 个假设逐项静态 + 动态排查 —— 全部排除，得出「疑似旧二进制」的错误结论；
  第二轮：加运行期日志，拿到真机日志后发现「pan 数值在变但画面不动」的决定性矛盾；
  第三轮：做渲染级验证，最终锁定真凶 —— QGraphicsView 默认 AlignCenter 自动居中，
          每帧把场景原点重新对齐到视口中心，整层覆盖了 setTransform 写入的平移量。
并附带记录测试方法论上走过的 5 次弯路，以及最终的修复与加固。
"""
import os
from docx import Document
from docx.shared import Pt, RGBColor, Cm
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH

FONT = "Microsoft YaHei"
DOC = "D:/01-CAD/02-avticationCAD/sketch-cad-cpp/docs/右键拖拽平移-深度思考.docx"

os.makedirs(os.path.dirname(DOC), exist_ok=True)
doc = Document()

# 默认字体
style = doc.styles['Normal']
style.font.name = FONT
style._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
style.font.size = Pt(10.5)

# 标题
T = doc.add_heading("航空CAD-视图交互-深度思考", level=0)
T.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = T.runs[0]
r.font.name = FONT
r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
r.font.size = Pt(18)
r.font.bold = True

SUB = doc.add_paragraph("右键拖拽「拖不动」：假设排查 · 日志实证 · 渲染级定位 · 根因与修复")
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


def note(text):
    """结论性强调段（深红）。"""
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.5)
    r = p.add_run(text)
    r.font.name = FONT
    r._element.rPr.rFonts.set(qn('w:eastAsia'), FONT)
    r.font.size = Pt(10.5)
    r.font.bold = True
    r.font.color.rgb = RGBColor(0xA8, 0x2B, 0x2B)
    return p


# ===================== 摘要 =====================
H("零、一句话结论", 1)
note("真凶不是事件、不是坐标、不是拦截，而是 QGraphicsView 的默认对齐方式 Qt::AlignCenter。")
P("右键拖拽的事件绑定、状态机、坐标增量、applyTransform 全部正确，panX_/panY_ 每帧都在正常变化；"
  "但 QGraphicsView 在每次绘制时会按 alignment() 与 sceneRect 把场景重新对齐到视口中心，"
  "这一步发生在 setTransform 之后，等于把我们刚写进去的平移量整层覆盖掉。"
  "于是出现「数据在动、画面不动」的现象——看起来就是「拖不动」。")
P("修复共三步，且缺一不可：① 固定一个任意缩放/平移下都远大于视口的场景矩形，让视图永远走「内容大于视口」分支、"
  "对齐缩进（leftIndent）恒为 0；② setTransformationAnchor(NoAnchor)，使 setTransform 不再经滚动条重定心、"
  "滚动条值恒为 0，矩阵平移成为唯一定位来源（双精度、1px 不丢）；③ 关闭滚动条，从源头杜绝滚动条值参与定位。"
  "对齐标志仅作保险。此后 setTransform 成为视图变换的唯一权威。")
code("scene_->setSceneRect(QRectF(-1e5, -1e5, 2e5, 2e5));   // ① 超大固定矩形：永远走内容>视口分支，indent 恒 0\n"
     "setAlignment(Qt::AlignLeft | Qt::AlignTop);          // 对齐标志仅作保险（取何值偏移都为 0）\n"
     "setTransformationAnchor(QGraphicsView::NoAnchor);    // ② setTransform 不再经滚动条重定心\n"
     "setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // ③ 关闭滚动条，杜绝其参与定位\n"
     "setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);")

# ===================== 第一节 =====================
H("一、问题现象与用户假设", 1)
P("现象：用户按住鼠标右键拖动时，画布上的图元、网格、坐标系等界面元素完全不跟随鼠标，"
  "整体呈「无响应」状态；而需求要求「右键拖动时视图跟着平移」。用户给出 4 个待排查假设：")
bullet("假设1：鼠标右键拖动的事件监听器是否正确绑定（是否真的接管了右键按下/移动/释放）。")
bullet("假设2：拖动事件是否被其它事件处理器拦截，或默认行为被阻止（如上下文菜单抢占右键）。")
bullet("假设3：是否存在 CSS 等价物（pointer-events / user-select）影响响应——这是 Web 概念，"
       "Qt Widgets 下对应 WA_TransparentForMouseEvents / setEnabled(false) / 事件过滤器。")
bullet("假设4：拖动逻辑中的坐标计算是否正确更新了元素位置（平移增量、变换矩阵是否正确）。")
P("本报告不靠「读代码觉得对」下结论，而是三轮递进实证：静态全仓排查 → 运行期真机日志 → 渲染级变换断言。")

# ===================== 第二节 =====================
H("二、第一轮：四项假设逐条排查（全部排除）", 1)

P("2.1 静态排查（覆盖假设 1 / 2 / 3）", bold=True)
P("全仓 grep 扫描 src/ 下与鼠标事件、上下文菜单、事件过滤、鼠标捕获、透明鼠标事件相关的所有写法：")
code("eventFilter | installEventFilter | contextMenuEvent | setContextMenuPolicy\n"
     "| WA_TransparentForMouseEvents | grabMouse | setMouseTracking | RightButton | CustomContextMenu")
P("结果：除 GraphicsView 自身主动处理右键外，全仓再无任何 eventFilter、无 WA_TransparentForMouseEvents、"
  "无 grabMouse。从「应用派发层」到「视图事件层」之间没有第三者拦截右键。")

P("2.2 假设1 — 事件监听器绑定：正确（不是病因）", bold=True)
code("protected:\n"
     "  void mousePressEvent(QMouseEvent* e)   override;\n"
     "  void mouseMoveEvent(QMouseEvent* e)    override;\n"
     "  void mouseReleaseEvent(QMouseEvent* e) override;")
P("右键按下把 panning_ / panRight_ 置真并记录 lastPan_；移动分支在 panning_ 为真时累加平移量；"
  "释放分支区分「单击（结束/取消绘制）」与「拖拽（仅结束平移）」。绑定与状态机均正确。")

P("2.3 假设2 — 事件被拦截 / 默认行为被阻止：不成立", bold=True)
bullet("全仓无事件过滤器、无 grabMouse、无透明鼠标事件属性。")
bullet("视图与视口均已 setContextMenuPolicy(Qt::NoContextMenu)，右键不弹菜单。")
bullet("BomTree 虽有自己的 contextMenuEvent，但那是另一控件，与画布右键无关。")

P("2.4 假设3 — CSS pointer-events / user-select 等价物：N/A（不成立）", bold=True)
bullet("pointer-events:none ≈ WA_TransparentForMouseEvents=true —— 全仓未使用。")
bullet("user-select —— 视图不处理文本选择，与右键拖拽无关。")
bullet("setEnabled(false) 会让控件整体不收事件 —— 视图始终 enabled。")

P("2.5 假设4 — 坐标计算：正确（不是病因）", bold=True)
code("// mouseMoveEvent 中（panning_ 为真时）\n"
     "panX_ += vp.x() - lastPan_.x();\n"
     "panY_ += vp.y() - lastPan_.y();\n"
     "lastPan_ = vp;\n"
     "applyTransform();\n"
     "\n"
     "// applyTransform：Y 取负以贴合标准笛卡尔（屏幕 Y 向上）\n"
     "setTransform(QTransform::fromTranslate(panX_, panY_).scale(zoom_, -zoom_));")
P("增量在同一坐标系（视口逻辑像素）内计算，delta 自洽。集成测试 ViewPanTest 用合成事件直接驱动处理器，"
  "对一次 (+40,+25) 的拖拽，transform().dx()/dy() 精确增加 (40,25)，右键单击不产生平移。")

note("第一轮结论（后被证伪）：4 项假设全部排除，一度判断为「用户运行了旧二进制」。"
     "这个结论错在——所有测试都只验证到「变换矩阵被正确写入」，从未验证「写入的变换真的驱动了渲染」。")

# ===================== 第三节 =====================
H("三、第二轮：运行期日志，抓到决定性矛盾", 1)
P("既然静态和单元层面都说「没问题」，就必须让真机自己说话。在 GraphicsView.cpp 顶部加一段 PAN_DEBUG 打点，"
  "把每一步事件现场写入 %TEMP%/sketchcad_pan.log，请用户运行带日志的版本后回传。")
code("#define PAN_DEBUG\n"
     "static void panDbg(const QString& msg) {   // 追加时间戳，逐条 flush，防崩溃丢日志\n"
     "    QFile& f = panDbgFile();               // %TEMP%/sketchcad_pan.log，Truncate 打开\n"
     "    if (f.isOpen()) { QTextStream ts(&f);\n"
     "        ts << QDateTime::currentDateTime().toString(\"hh:mm:ss.zzz\") << \" \" << msg << \"\\n\";\n"
     "        ts.flush(); }\n"
     "}")

P("3.1 用户回传日志的关键片段（342 行）", bold=True)
code("[press]  右键 -> panning_=true panRight_=true lastPan=(...) tool=...\n"
     "[move]   平移中 dx=.. dy=..  pan(48,..)->(..)  zoom=4.00      <- 数值确实在变\n"
     "[move]   平移中 ...          pan 一路变到 (-82, ..)\n"
     "[release] 右键释放 panning_=true wasClick=0 -> 仅结束平移\n"
     "[raw]    ContextMenu 事件触发（右键菜单手势）")

P("3.2 从日志读出的三条信息", bold=True)
bullet("① 右键平移逻辑完全在跑：press 置位、move 每帧累加、pan 数值从 48 一路变到 −82、release 正确收尾。"
       "换句话说，「事件到了、状态对了、算术对了」——但用户看到的画面纹丝不动。这是决定性矛盾：数据在动，画面不动。")
bullet("② 日志第 118~170 行出现「没有任何 [press]，却持续 [move] 平移中」，且 zoom 从 4.00 跳到 4.40。"
       "说明 panning_ 曾卡死在 true（上一轮 release 被吞）。这是一个真实但次要的健壮性缺陷。")
bullet("③ [raw] 打点只抓到 2 次 ContextMenu，完全没有 Press / Move / Wheel。这一条一开始被误读为"
       "「事件被上层吞了」，后来查明是打点位置错了——见 3.3。")

P("3.3 打点位置的错误：event() 对鼠标事件是失明的", bold=True)
P("QGraphicsView 继承自 QAbstractScrollArea，鼠标与滚轮事件由视口控件 viewport() 接收，"
  "先进入 QGraphicsView::viewportEvent()，再被转发到 mousePressEvent 等；它们根本不会经过 view 自身的 event() 重写。"
  "所以把钩子挂在 event() 上必然只能看到 ContextMenu 这类直接发给 view 的事件。修正为重写 viewportEvent()：")
code("bool GraphicsView::viewportEvent(QEvent* e) {\n"
     "#ifdef PAN_DEBUG\n"
     "    switch (e->type()) {\n"
     "    case QEvent::MouseButtonPress:  case QEvent::MouseButtonRelease:\n"
     "    case QEvent::MouseMove: { /* 记录 [raw] Press/Release/Move + button/buttons/pos */ }\n"
     "    case QEvent::ContextMenu: panDbg(\"[raw] ContextMenu ...\"); break;\n"
     "    case QEvent::Wheel:       panDbg(\"[raw] Wheel\");           break;\n"
     "    default: break; }\n"
     "#endif\n"
     "    return QGraphicsView::viewportEvent(e);\n"
     "}")
note("方法论教训：诊断工具本身也需要被验证。一个挂错位置的日志钩子，会把「我没看见」错误地"
     "呈现为「它没发生」，直接把排查引向错误方向。")

# ===================== 第四节 =====================
H("四、第三轮：渲染级验证，锁定真凶", 1)
P("矛盾已经收窄到一个点：setTransform 把平移量写进去了，为什么画面不动？"
  "唯一的解释是——变换被写入之后，又被别的东西改掉了。为此新建 ViewRenderTest，"
  "不再断言我们自己的 panX_/panY_，而是断言 QGraphicsView 最终生效的底层变换 view.transform()。")

P("4.1 测试方法论：走过的 5 次弯路（值得记录）", bold=True)
bullet("① 用 viewport()->render() 截图算灰度质心 → 背景网格上下左右对称，质心恒在正中，得到「假 FAIL」。")
bullet("② 改用饱和度质心 + view.render() + processEvents() → 仍 FAIL。")
bullet("③ 加一个「朴素 QGraphicsView + 红方块」的校准块：连原生 QGraphicsView 手动 setTransform(120,90) "
       "后截图 shift 也是 0 → 证明 offscreen 平台下 render() 会按 sceneRect 重新适配，根本不反映 transform。截图法整体作废。")
bullet("④ 改用 mapFromScene(0,0) 观察屏幕坐标 → offscreen 下视图未真正布局，数值在 126 / 0 / 126 之间抖动，不可靠。")
bullet("⑤ 最终正确做法：直接断言 view.transform() 的 m31 / m32。这是 Qt 内部确定性的状态，"
       "不依赖窗口系统、不依赖是否真的画出来，也正是「渲染时实际使用的变换」。")

P("4.2 决定性证据", bold=True)
P("在修复之前，测试打出的是：")
code("[RENDER-A] shift=(0.0, 0.0)    <- 手动 setTransform(120,80) 之后，实际生效的变换平移量仍是 0")
P("这一行铁证：setTransform 写入的平移量没有存活到渲染阶段，被某个机制清掉了。")

P("4.3 机理：QGraphicsView 的自动居中", bold=True)
P("QGraphicsView 的 alignment() 默认值是 Qt::AlignCenter。当场景矩形（sceneRect）在某个方向上小于视口时，"
  "视图会在每次布局/绘制时，按 alignment 把场景重新摆放到视口中央——具体做法就是覆写内部的变换偏移量。"
  "这一步发生在我们 setTransform 之后，因此：")
bullet("我们写入 (panX_, panY_)；")
bullet("视图随即按「让场景居中」重算偏移，把它换成一个只与 sceneRect / 视口尺寸有关的值；")
bullet("由于场景内容与视口尺寸在拖拽过程中都没变，这个值是常量 → 画面锁死在正中，纹丝不动。")
note("这就是「panX_ 从 48 变到 −82，但画面一动不动」的完整解释：我们的平移量每帧都被居中逻辑覆盖掉了。")

P("4.4 一个隐蔽的坑：空标志会被当作居中", bold=True)
P("第一次修复时写的是 setAlignment(Qt::Alignment())，期望「不做任何对齐」。但 Qt 内部对水平/垂直分别判断，"
  "「既不是 Left/Right 也不是 Top/Bottom」的空标志会走 else 分支，即按居中处理。必须显式给出具体标志：")
code("// 错误：空标志 = 居中\n"
     "setAlignment(Qt::Alignment());\n"
     "// 正确：显式左上对齐\n"
     "setAlignment(Qt::AlignLeft | Qt::AlignTop);")
P("改为左上对齐后，测试从 shift=(0,0) 变为 [A] after=(126, 7) —— 居中被打破，但 y 方向仍有残留偏移。"
  "最初的想法是「把 sceneRect 一并清空，让对齐失去作用对象」，但这条路走不通：空矩形 QRectF() 会被 Qt 回退为"
  "itemsBoundingRect()，当场景内容很小时又落入「内容<视口」分支，其中 AlignLeft 的 leftIndent=-viewRect.left()"
  "恰好把矩阵平移量整层消掉，现象依旧是「数据在变、画面不动」。最终稳妥解见 5.2 的三步修复。")

# ===================== 第五节 =====================
H("五、根因结论与修复", 1)
P("5.1 根因（一句话）", bold=True)
note("QGraphicsView 默认 Qt::AlignCenter 的自动居中，在每帧绘制时依据 sceneRect 把场景原点重新对齐到视口中心，"
     "覆盖了 applyTransform() 通过 setTransform 写入的平移量，导致「pan 数值在变、画面不动」的「拖不动」现象。")

P("5.2 修复（GraphicsView 构造函数，三步，缺一不可）", bold=True)
code("scene_ = new QGraphicsScene(this);\n"
     "setScene(scene_);\n"
     "// 关键：让平移/缩放完全由 applyTransform() 的 setTransform 控制，杜绝 QGraphicsView 的自动居中。\n"
     "// 对照 Qt 6.10.1 qgraphicsview.cpp 逐行确认：viewport = matrix.map(scene) - scroll，\n"
     "//   scrollX = hbar->value() - leftIndent。矩阵平移能否生效，取决于 indent 与滚动条值是否被改写：\n"
     "// ① 固定一个任何缩放/平移下都远大于视口的场景矩形 -> 永远走「内容>视口」分支，leftIndent 恒为 0；\n"
     "// ② NoAnchor -> setTransform 不再经滚动条重定心（centerView 被禁用），滚动条值恒为 0，scroll 恒 0；\n"
     "// ③ 关闭滚动条 -> 既无可见 UI，也从源头杜绝任何滚动条值参与定位。\n"
     "scene_->setSceneRect(QRectF(-1e5, -1e5, 2e5, 2e5));\n"
     "setAlignment(Qt::AlignLeft | Qt::AlignTop);   // 仅作保险：大矩形下对齐偏移恒为 0\n"
     "setTransformationAnchor(QGraphicsView::NoAnchor);\n"
     "setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);\n"
     "setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);\n"
     "setMouseTracking(true);")
P("三步缺一不可：① 断掉「内容<视口」分支（该分支的 leftIndent 会整层消掉矩阵平移）；② 禁用 setTransform 内部"
  "的 centerView 重定心；③ 关闭滚动条，使 scroll 恒为 0。最终矩阵的平移分量成为视图定位的唯一来源，"
  "平移与缩放严格跟手，且双精度保留、1px 不丢。")

P("5.3 修复后的渲染级验证", bold=True)
code("[A] view.transform() after setTransform(120,80): m31=120.0 m32=80.0\n"
     "[A] setTransform 精确写入底层变换: YES\n"
     "[B] transform delta after right-drag: dx=120.0 dy=80.0\n"
     "[B] 右键拖拽精确更新底层变换: YES\n"
     "RESULT: PASS")
P("[A] 验证「手动写入的变换能原样存活」；[B] 用真实右键拖拽事件走完整链路，"
  "从 (400,300) 拖动 (120,80)，底层变换的平移增量精确为 (120,80)。"
  "注意 [B] 之前必须先 fitView() 让 panX_/panY_ 与当前变换一致，否则拖拽增量会叠加在陈旧基准上（这是测试早期的一个自伤 bug）。")

# ===================== 第六节 =====================
H("六、附带修复：平移状态卡死的兜底恢复", 1)
P("日志揭示的次要缺陷：出现过「没有 [press] 却持续 [move] 平移中」，说明某次 release 被上下文菜单流程或系统吞掉，"
  "panning_ 卡在 true，之后鼠标一动视图就乱飘。加两处兜底复位：")

P("6.1 移动时：没有任何按键按住却处于平移态 → 立刻复位", bold=True)
code("// 卡死态兜底恢复：panning_ 为真却没有按键按住（且非「空格+左键」拖动），\n"
     "// 说明上一轮释放事件被上下文菜单/系统吞掉、panning_ 未复位。\n"
     "if (panning_ && e->buttons() == Qt::NoButton && !(spaceDown_ && (e->buttons() & Qt::LeftButton))) {\n"
     "    panning_ = false; panRight_ = false; rightMoved_ = false;\n"
     "    setCursor(Qt::ArrowCursor);\n"
     "    panDbg(\"[move] 兜底复位 panning_（卡死态恢复）...\");\n"
     "    return;\n"
     "}")

P("6.2 上下文菜单触发时：若正在平移 → 复位后再 ignore()", bold=True)
code("void GraphicsView::contextMenuEvent(QContextMenuEvent* e) {\n"
     "    if (panning_) {\n"
     "        panning_ = false; panRight_ = false; rightMoved_ = false;\n"
     "        setCursor(Qt::ArrowCursor);\n"
     "        panDbg(\"[ctx] 平移中触发上下文菜单，兜底复位 panning_\");\n"
     "    }\n"
     "    e->ignore();\n"
     "}")
P("这两处属于「自愈式」防御：即使未来某个平台/Qt 版本再次吞掉释放事件，视图也只会短暂异常一帧，"
  "下一次鼠标移动即自动恢复正常，不会持续卡在平移态。")

# ===================== 第七节 =====================
H("七、回归验证", 1)
P("改动后完整重建全部目标并运行三套测试，结果全绿：")
bullet("SketchCAD / ViewPanTest / ViewRenderTest / SketchSelfTest：构建 0 错误 / 0 警告（含 -Wall -Wextra）。")
bullet("ViewRenderTest：PASS —— [A] m31=120 m32=80；[B] 右键拖拽 delta=(120,80)，精确匹配。")
bullet("ViewPanTest：PASS —— 直接调 handler 拖拽 (40,25) 精确匹配；右键单击不平移；"
       "经 QApplication::sendEvent 走真实派发链路拖拽 (60,40) 精确匹配；viewportEvent 打点抓到原始 Press / Move。")
bullet("SketchSelfTest：174 项通过 / 0 失败，既有功能无回归。")
P("构建与运行命令（需把 Qt bin 加入 PATH）：")
code("export PATH=\"/c/Qt/6.10.1/mingw_64/bin:$PATH\"\n"
     "cmake --build build --target SketchCAD ViewPanTest ViewRenderTest SketchSelfTest\n"
     "QT_QPA_PLATFORM=offscreen ./build/ViewRenderTest.exe\n"
     "QT_QPA_PLATFORM=offscreen ./build/ViewPanTest.exe\n"
     "QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 ./build/SketchSelfTest.exe")
P("补充：ViewPanTest 的真实派发段必须把事件发给 viewport() 而非 view 本身——"
  "QGraphicsView 的鼠标事件由视口接收，发给 view 既不经过 viewportEvent 打点，也不会转发到鼠标处理器。"
  "这正是修正打点位置后该测试一度失败的原因。")

# ===================== 第八节 =====================
H("八、代码落点总览", 1)
bullet("src/ui/GraphicsView.cpp（核心修复）：构造函数三步禁用自动居中 —— 固定超大场景矩形 "
       "setSceneRect(QRectF(-1e5,-1e5,2e5,2e5)) + setAlignment(AlignLeft|AlignTop) + "
       "setTransformationAnchor(NoAnchor) + 关闭横纵滚动条；event() 重写改为 viewportEvent()（修正打点位置）；"
       "mouseMoveEvent 新增卡死态兜底复位；contextMenuEvent 新增平移态兜底复位；顶部 PAN_DEBUG 打点段。")
bullet("src/ui/GraphicsView.h：bool event(QEvent*) override 改为 bool viewportEvent(QEvent*) override。")
bullet("tests/view_render_test.cpp（新增）：渲染级验证，断言 view.transform() 的 m31/m32；"
       "含 [A] 手动 setTransform 与 [B] 真实右键拖拽两段。")
bullet("tests/view_pan_test.cpp：右键拖拽/单击集成测试；新增经 sendEvent 到 viewport() 的真实派发段，"
       "同时断言平移增量与 viewportEvent 打点命中。")
bullet("CMakeLists.txt：新增 ViewRenderTest 可执行目标（GraphicsView + 核心层源码，链接 Qt6::Widgets，-Wall -Wextra）。")
bullet("tools/gen_deepthink_rightdrag.py：本文件生成脚本。")

# ===================== 第九节 =====================
H("九、复盘：这次排查为什么绕了远路", 1)
P("9.1 三个认知陷阱", bold=True)
bullet("① 验证层次不足。前两轮所有测试都只验证到「我们的状态变量与我们调用的 API 参数是对的」，"
       "却没有验证「框架最终采用的值是不是这个」。只要中间存在框架的后置改写，这类测试就会全绿而 bug 依旧。"
       "教训：断言要下沉到「最终生效的状态」，而不是「我方写入的意图」。")
bullet("② 诊断工具未经校验。日志钩子挂在 event() 上，对鼠标事件天然失明，"
       "却输出了「没有收到 Press」这样极具误导性的证据，把排查推向「事件被吞」的错误分支。"
       "教训：新增诊断手段后，先用一个已知必然发生的事件校验它确实能抓到。")
bullet("③ 测量方法不适配环境。offscreen 平台下 render() 会按 sceneRect 重新适配、mapFromScene 因未真正布局而抖动，"
       "两者都不反映真实变换。用一个「朴素 QGraphicsView + 红方块」的校准块，才快速证明是测量方法坏了而非被测对象坏了。"
       "教训：当测量结果反直觉时，先给测量工具做一次已知答案的校准实验。")

P("9.2 沉淀下来的排查套路", bold=True)
bullet("现象矛盾优先：「数据在变、画面不动」这类矛盾，几乎必然指向「渲染管线上有人在我们之后改写了状态」，"
       "应立刻从「我们的逻辑对不对」转向「谁在我们之后动了手脚」。")
bullet("框架默认值是头号嫌疑：QGraphicsView 的 AlignCenter、sceneRect 自动扩张、ScrollBar 策略，"
       "都会在你不知情时改写视图变换。凡是要自己完全掌控 transform 的场景，先把这些默认行为逐一关掉。")
bullet("真机日志不可替代：静态分析与单元测试都无法暴露本例根因，只有真机日志给出了那条决定性矛盾。")

P("9.3 遗留与后续", bold=True)
bullet("PAN_DEBUG 打点当前保持常开（写 %TEMP%/sketchcad_pan.log，逐条 flush，对性能影响可忽略）。"
       "确认线上稳定后可注释掉源码顶部的 #define PAN_DEBUG 一行即可整体关闭。")
bullet("可扩展：把「右键拖拽平移」做成可配置（改用中键/空格，或右键只平移不结束绘制），适配不同操作习惯。")

P("")
pf = P("文档生成时间：2026-09-08（第三版：修复由「清空 sceneRect + 左上对齐」两行更正为「超大场景矩形 + NoAnchor + 关闭滚动条」三步，"
  "根因仍为「AlignCenter 自动居中覆盖 setTransform」）", size=9)
pf.runs[0].font.color.rgb = RGBColor(0x80, 0x80, 0x80)

doc.save(DOC)
print(f"Saved: {DOC}  ({os.path.getsize(DOC)} bytes)")
