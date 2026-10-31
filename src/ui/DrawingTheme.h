// DrawingTheme.h —— 绘图区域（画布）主题配色
//
// 与 AppSettings 的 display/theme（light/dark/system）以及 ThemeManager 的
// 生效模式（effectiveMode）联动：current() 按生效模式返回 light / dark 两套
// 配色集之一。所有「屏幕上」的绘图区视觉元素（画布背景、网格、坐标轴、原点、
// 各类引导线、HUD 文字、当前图层徽标、选中/预览/告警高亮、约束图标、图层默认
// 色、框选/夹点/区域拾取等）统一从这里取色，实现「绘图区域随主题同步翻色、切换
// 过程平滑无闪烁」。
//
// 注意：打印（PlotRenderer）走纸张配色，不读取本主题，故打印永远白底黑线。
//
#pragma once

#include <QColor>

namespace SketchCAD {

// 一套完整绘图区配色。light() 刻意保留原版亮色外观，dark() 为暗色等新配色。
struct DrawingPalette {
    // —— 画布与坐标系 ——
    QColor canvas;        // 画布背景（= ThemeManager::canvasColor()）
    QColor gridMinor;     // 次网格线
    QColor gridMajor;     // 主网格线
    QColor axisX;         // X 轴（箭头+文字）
    QColor axisY;         // Y 轴（箭头+文字）
    QColor originCross;   // 原点十字点
    QColor axisLabel;     // 坐标轴 X / Y / O 文字

    // —— 选中 / 预览 / 告警 ——
    QColor selection;     // 选中高亮（蓝）
    QColor preview;       // 预览图元（半透明）
    QColor warn;          // 过约束 / 冲突高亮（红）

    // —— 引导线（构造辅助 / 极轴 / 正交 / 对象追踪 / 临时追踪 / 追踪落点）——
    QColor guide;         // 通用构造辅助虚线
    QColor objTrackGuide; // 对象追踪对齐线
    QColor polarGuide;    // 极轴射线
    QColor tempTkGuide;   // 临时追踪点（TK）
    QColor orthoGuide;    // 正交引导线
    QColor trackDropPolar;// 追踪落点：极轴
    QColor trackDropObj;  // 追踪落点：对象追踪
    QColor trackDropTk;   // 追踪落点：临时追踪点

    // —— HUD 文字与状态行 ——
    QColor hudText;        // 工作平面 / 坐标读数
    QColor hudAccentBlue;  // 捕捉开关行
    QColor hudAccentOrange;// 追踪状态行
    QColor hudAccentGreen; // 输入约束行

    // —— 当前图层徽标 ——
    QColor badgeBg;
    QColor badgeBorder;
    QColor badgeText;

    // —— 框选 / 窗口缩放 / 区域拾取 / 夹点 ——
    QColor marquee;         // 框选橡皮筋
    QColor marqueeFill;
    QColor zoomBox;         // 窗口缩放框
    QColor zoomBoxFill;
    QColor regionPick;      // 区域拾取高亮
    QColor regionPickFill;
    QColor grip;            // 夹点 / 热夹点暖色块
    QColor gripLine;        // 夹点描边
    QColor trimBound;       // 已指定的剪切边界

    // —— 约束可视化 ——
    QColor constraintOk;    // 已满足
    QColor constraintWarn;  // 未满足 / 待定

    // —— 图层默认色（图层无颜色时回退）——
    QColor layerDefault;

    // —— 图纸正文墨色（ACI 7 语义，随主题翻转）——
    // 正文线/轮廓色为 aciInkMark()（DXF ACI 7/255、图元默认色）时按本字段解析：
    // 亮主题=黑、暗主题=白（对应 AutoCAD 系「正文色随背景反色」的显示语义）。
    QColor paperInk;
};

class ThemeManager; // 前向声明，避免 DrawingTheme 反向依赖 ThemeManager 头文件

class DrawingTheme {
public:
    using Palette = DrawingPalette; // GraphicsView.cpp 等以 DrawingTheme::Palette 引用

    // 两套静态配色（进程级单例，零构造开销）
    static const DrawingPalette& light();
    static const DrawingPalette& dark();

    // 按 ThemeManager::instance()->effectiveMode() 取当前生效配色。
    // System 模式由 ThemeManager 解析为实际 Light/Dark。
    static const DrawingPalette& current();

    // 把图元颜色按当前主题解析：等于 ACI 7 语义标记（aciInkMark()）→ 返回
    // current().paperInk（亮主题黑、暗主题白）；其余颜色原样返回。
    // 屏幕渲染的所有取色点（GraphicsView）统一走这里；打印端不走（白底黑线）。
    static QColor resolveInk(const QColor& c);
};

} // namespace SketchCAD
