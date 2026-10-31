#pragma once
// ============================================================================
// ZwcadTextLayout —— 中望CAD(ZWCAD)/AutoCAD 兼容的文字排版引擎（可复用模块）
// ----------------------------------------------------------------------------
// 目标：以「内容 + 格式参数 → 行盒子」的纯函数接口，准确还原 ZWCAD 文字在
// 字符对齐、行间距、自动换行、段落缩进与整体布局上的格式规范：
//
//   ① 9 点对齐（MTEXT 组码 71 = attach+1；TEXT 72/73 同映射）：
//      水平 左/中/右 × 垂直 上/中/下。锚点 = 对齐点：
//        垂直「上」→ 块在锚点下方（首行顶边贴锚点）
//        垂直「中」→ 块关于锚点垂直居中
//        垂直「下」→ 块在锚点上方（末行底边贴锚点）
//      ⚠️ 历史 bug 修复：旧渲染端 yc = startYv − (i+0.5)·lineH 写在「局部 y
//      向下」坐标系里却按「y 向上」推导 → 块整体画到锚点上方且行序颠倒
//      （末行在顶）。本模块统一为排版惯例「Y 向下为正」，渲染端只逐行画
//      LineBox，不再自行推导锚点数学（三站点不可能再分叉）。
//
//   ② 行间距（MTEXT 组码 44 行距因子 + 组码 73 行距样式）：
//      行槽高(基线距) = factor × (5/3) × 字高 —— DXF 规范的「3-on-5」默认
//      单倍行距（3 份字高墨迹 + 2 份空隙 = 5 份基线距），factor 合法域
//      [0.25, 4.0]。单行 TEXT 无行距概念（行槽 = 字高）。
//
//   ③ 自动换行（MTEXT 组码 41 列宽）：贪心折行，CJK 任意字间可断、拉丁按
//      空格/连字符/标点断词、超长词硬断（度量经 TextMetrics，三处同源）。
//
//   ④ 段落与缩进：'\n' 与 MTEXT 软段标记 \P 均为段落分隔；\p...; 段落属性
//      解析首行缩进 i / 整体左缩进 c（单位 l = 行槽高，缺省 = mm）；
//      其余行内格式码（\C \c \H \W \Q \T \A \F \L \l \O \o \K \N 与 {} 分组）
//      按规范剥离，避免把 ZWCAD 源码当字面渲染。\S 堆叠分数保持既有行为
//      （字面渲染 + 明确提示，见 TextMetrics::containsStackedFraction）。
//
// 无头安全：不依赖 QGuiApplication（度量降级路径在 TextMetrics 内），自测可直测。
// 单位：全部为世界 mm；Y 轴约定见 LineBox::yCenterMm。
// ============================================================================
#include <QString>
#include <QVector>
#include <algorithm>
#include "TextStyle.h"   // ResolvedTextStyle

namespace SketchCAD {
namespace ZwcadText {

// ZWCAD/AutoCAD 默认单倍行距系数（DXF「3-on-5」：基线距 = 5/3 × 字高）
inline constexpr double kSingleSpacing = 5.0 / 3.0;

// ---- 输入：格式参数 -------------------------------------------------------
struct Format {
    double heightMm          = 3.5;   // 字高(mm)（DXF 组码 40 语义）
    double lineSpacingFactor = 1.0;   // 行距因子（DXF MTEXT 组码 44，[0.25,4.0]，1=单倍）
    int    attach            = 1;     // 9 点对齐 0..8（= MTEXT 组码 71 − 1；1=中上）
    double wrapWidthMm       = 0.0;   // 自动换行宽度(mm)（DXF MTEXT 组码 41；0 = 仅显式换行）
};

// ---- 输出：单行盒子（相对锚点，X 右正、Y **向下**为正 —— 排版惯例）--------
struct LineBox {
    QString text;
    double  xMm       = 0;   // 行左缘相对锚点 X（已含 9 点对齐与段落缩进偏移）
    double  yCenterMm = 0;   // 行槽垂直中心相对锚点 Y（向下为正；渲染端直接作局部 y）
    double  widthMm   = 0;   // 该行渲染宽度(mm)
};

// ---- 输出：整块布局 -------------------------------------------------------
struct Result {
    QVector<LineBox> lines;
    double slotHeightMm = 0;   // 行槽高(基线距) = factor × (5/3) × 字高（单行 = 字高）
    double widthMm      = 0;   // 最宽行宽(mm)
    double heightMm     = 0;   // 块高(mm) = lines.size() × slotHeightMm
};

// ---- 段落解析输出（公开以便测试与上层复用）--------------------------------
struct Paragraph {
    QString text;               // 已剥离行内格式码的纯文本（无 \n）
    double  firstIndentMm = 0;  // 首行缩进(mm)（\p i 值；l 单位已换算）
    double  leftIndentMm  = 0;  // 整段左缩进(mm)（\p c 值；l 单位已换算）
};

// ---- 内容解析：MText 内容 → 段落列表 --------------------------------------
// '\n' 与 \P 为段落分隔；\p...; 解析 i/c 缩进；其余行内格式码剥离；
// 连续空段落保留为空行（ZWCAD 语义：\P\P = 空行）。
QVector<Paragraph> parseParagraphs(const QString& content, double slotHeightMm);

// ---- 多行布局（MTEXT 规范：段落/缩进/换行/行距/9 点对齐）------------------
Result layoutMText(const QString& content, const Format& f, const ResolvedTextStyle& style);

// ---- 单行布局（TEXT 规范：无行距/缩进概念，行槽 = 字高，9 点对齐）---------
Result layoutSingleLine(const QString& content, const Format& f, const ResolvedTextStyle& style);

// ---- 行距因子合法域钳制（DXF 组码 44：0.25~4.0）---------------------------
inline double clampSpacingFactor(double v) { return std::clamp(v, 0.25, 4.0); }

} // namespace ZwcadText
} // namespace SketchCAD
