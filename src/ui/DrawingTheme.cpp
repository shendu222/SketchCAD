// DrawingTheme.cpp —— 绘图区域两套主题配色的具体取值
//
// light() 刻意复用原版（改造前）的亮色外观，保证切换主题「只新增暗色、不改变
// 原有亮色观感」；dark() 为针对暗色画布（#26282D）专门调整的可见配色。
//
#include "ui/DrawingTheme.h"
#include "ui/ThemeManager.h"
#include "core/Element.h"   // aciInkMark()：ACI 7 正文色语义标记（resolveInk 识别用）

namespace SketchCAD {

const DrawingPalette& DrawingTheme::light() {
    static const DrawingPalette p = {
        /* canvas */         QColor(245, 247, 250),
        /* gridMinor */      QColor(244, 182, 182),
        /* gridMajor */      QColor(230, 138, 138),
        /* axisX */          QColor(200, 60, 60),
        /* axisY */          QColor(40, 150, 80),
        /* originCross */    QColor(70, 74, 84),
        /* axisLabel */      QColor(70, 74, 84),

        /* selection */      QColor(31, 95, 139),
        /* preview */        QColor(31, 95, 139, 215),
        /* warn */           QColor(214, 48, 49),

        /* guide */          QColor(120, 140, 170, 170),
        /* objTrackGuide */  QColor(0, 150, 220, 170),
        /* polarGuide */     QColor(220, 140, 0, 170),
        /* tempTkGuide */    QColor(0, 170, 170),
        /* orthoGuide */     QColor(90, 100, 120, 150),
        /* trackDropPolar */ QColor(230, 150, 0),
        /* trackDropObj */   QColor(0, 150, 220),
        /* trackDropTk */    QColor(0, 170, 170),

        /* hudText */        QColor(108, 116, 132),
        /* hudAccentBlue */  QColor(70, 130, 255),
        /* hudAccentOrange */QColor(190, 140, 40),
        /* hudAccentGreen */ QColor(80, 120, 90),

        /* badgeBg */        QColor(255, 255, 255, 235),
        /* badgeBorder */    QColor(120, 130, 150),
        /* badgeText */      QColor(28, 32, 42),

        /* marquee */        QColor(70, 130, 255),
        /* marqueeFill */    QColor(70, 130, 255, 28),
        /* zoomBox */        QColor(30, 160, 90),
        /* zoomBoxFill */    QColor(30, 160, 90, 24),
        /* regionPick */     QColor(255, 140, 0),
        /* regionPickFill */ QColor(255, 140, 0, 40),
        /* grip */           QColor(230, 145, 56),
        /* gripLine */       QColor(31, 95, 139),
        /* trimBound */      QColor(214, 69, 65, 200),

        /* constraintOk */   QColor(30, 150, 110),
        /* constraintWarn */ QColor(220, 120, 30),

        /* layerDefault */   QColor(26, 29, 40),

        /* paperInk */       QColor(0, 0, 0),     // 亮主题：正文线=黑
    };
    return p;
}

const DrawingPalette& DrawingTheme::dark() {
    // 暗色画布底色 #26282D = (38,40,45)。所有结构色提亮以保证对比度。
    static const DrawingPalette p = {
        /* canvas */         QColor(38, 40, 45),
        /* gridMinor */      QColor(72, 76, 86),
        /* gridMajor */      QColor(104, 110, 122),
        /* axisX */          QColor(232, 98, 98),
        /* axisY */          QColor(74, 200, 124),
        /* originCross */    QColor(176, 184, 196),
        /* axisLabel */      QColor(176, 184, 196),

        /* selection */      QColor(92, 162, 220),
        /* preview */        QColor(92, 162, 220, 215),
        /* warn */           QColor(242, 98, 98),

        /* guide */          QColor(150, 168, 196, 170),
        /* objTrackGuide */  QColor(96, 176, 242, 180),
        /* polarGuide */     QColor(242, 172, 44, 180),
        /* tempTkGuide */    QColor(60, 200, 200),
        /* orthoGuide */     QColor(150, 162, 186, 160),
        /* trackDropPolar */ QColor(242, 172, 44),
        /* trackDropObj */   QColor(96, 176, 242),
        /* trackDropTk */    QColor(60, 200, 200),

        /* hudText */        QColor(182, 190, 202),
        /* hudAccentBlue */  QColor(120, 172, 255),
        /* hudAccentOrange */QColor(236, 182, 72),
        /* hudAccentGreen */ QColor(122, 192, 134),

        /* badgeBg */        QColor(30, 34, 42, 235),
        /* badgeBorder */    QColor(96, 106, 124),
        /* badgeText */      QColor(226, 231, 240),

        /* marquee */        QColor(120, 172, 255),
        /* marqueeFill */    QColor(120, 172, 255, 34),
        /* zoomBox */        QColor(72, 200, 124),
        /* zoomBoxFill */    QColor(72, 200, 124, 30),
        /* regionPick */     QColor(255, 172, 50),
        /* regionPickFill */ QColor(255, 172, 50, 48),
        /* grip */           QColor(242, 172, 84),
        /* gripLine */       QColor(92, 162, 220),
        /* trimBound */      QColor(242, 112, 108, 210),

        /* constraintOk */   QColor(62, 202, 142),
        /* constraintWarn */ QColor(242, 162, 64),

        /* layerDefault */   QColor(212, 216, 224),

        /* paperInk */       QColor(255, 255, 255), // 暗主题：正文线=白
    };
    return p;
}

const DrawingPalette& DrawingTheme::current() {
    // ThemeManager::effectiveMode() 已把 System 解析为实际 Light/Dark。
    if (ThemeManager::instance()->effectiveMode() == ThemeManager::Mode::Dark)
        return dark();
    return light();
}

QColor DrawingTheme::resolveInk(const QColor& c) {
    // ACI 7 / 255 / 图元默认色（统一为 aciInkMark 语义标记）按当前主题翻转；
    // 其余颜色（含中心线天蓝、显式色号）原样使用。
    if (c == aciInkMark())
        return current().paperInk;
    return c;
}

} // namespace SketchCAD
