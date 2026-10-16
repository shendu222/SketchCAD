#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QFont>
#include <QString>
#include <cmath>
#include "../core/Element.h"          // LineStyle / toPenStyle
#include "../core/LineStyleConfig.h"  // lineStyleIsThick（粗/细档）

// ---------------------------------------------------------------------------
// 图标库：全部用 QPainter 在代码里矢量绘制，不使用 .qrc / png 外部资源。
//
// 为什么这样做（而不是塞一堆 png）：
//  1) 交付物仍然是"一个 exe + Qt 运行库"，不会散落图片资源，也不会出现
//     资源路径在打包后失效的经典问题；
//  2) 图标按 devicePixelRatio 重新光栅化，高分屏（125%/150%/200%）不发虚；
//  3) 工具条与「草图绘制」菜单共用同一个 QAction，图标只在这里定义一次，
//     天然保证"菜单图标 == 工具条图标"，不会两边改漏。
//
// 画布统一 24x24 逻辑像素，内容留 3px 边距。
// ---------------------------------------------------------------------------

namespace SketchCAD {
namespace icons {

inline QColor inkColor()   { return QColor(0x1F, 0x33, 0x52); }  // 主几何线条
inline QColor markColor()  { return QColor(0xD1, 0x4B, 0x3F); }  // 控制点 / 强调
inline QColor guideColor() { return QColor(0x9A, 0xA6, 0xB8); }  // 辅助线 / 虚线

namespace detail {

constexpr double kSize = 24.0;

inline void paintGlyph(QPainter& g, const QString& id)
{
    const QColor ink = inkColor(), mark = markColor(), guide = guideColor();

    QPen pen(ink, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    g.setPen(pen);
    g.setBrush(Qt::NoBrush);

    // --- 局部小工具 ---
    auto dot = [&g](double x, double y, const QColor& c, double r = 1.7) {
        g.save(); g.setPen(Qt::NoPen); g.setBrush(c);
        g.drawEllipse(QPointF(x, y), r, r); g.restore();
    };
    auto dash = [&g, &guide](double x1, double y1, double x2, double y2) {
        g.save(); QPen p(guide, 1.0); p.setStyle(Qt::DashLine); p.setDashPattern({3, 2});
        g.setPen(p); g.drawLine(QPointF(x1, y1), QPointF(x2, y2)); g.restore();
    };
    auto text = [&g](const QString& t, const QColor& c, double px, const QRectF& r) {
        g.save(); QFont f = g.font(); f.setPixelSize(int(px)); f.setBold(true);
        g.setFont(f); g.setPen(c);
        g.drawText(r, Qt::AlignCenter, t); g.restore();
    };
    // 在 (x,y) 处沿角度 ang(弧度) 画一个实心箭头
    auto arrow = [&g](double x, double y, double ang, const QColor& c, double len = 4.6) {
        const double w = len * 0.46;
        QPointF tip(x, y);
        QPointF b1(x - len * std::cos(ang) - w * std::sin(ang),
                   y - len * std::sin(ang) + w * std::cos(ang));
        QPointF b2(x - len * std::cos(ang) + w * std::sin(ang),
                   y - len * std::sin(ang) - w * std::cos(ang));
        g.save(); g.setPen(Qt::NoPen); g.setBrush(c);
        g.drawPolygon(QPolygonF{tip, b1, b2}); g.restore();
    };
    auto magnifier = [&](bool plus, bool none) {
        g.drawEllipse(QPointF(10.0, 10.0), 6.0, 6.0);
        g.drawLine(QPointF(14.4, 14.4), QPointF(20.0, 20.0));
        g.save(); g.setPen(QPen(mark, 1.8, Qt::SolidLine, Qt::RoundCap));
        if (!none) {
            g.drawLine(QPointF(7.0, 10.0), QPointF(13.0, 10.0));
            if (plus) g.drawLine(QPointF(10.0, 7.0), QPointF(10.0, 13.0));
        }
        g.restore();
    };

    // ================= 绘图工具 =================
    if (id == "select") {                       // 箭头光标
        QPolygonF a{{6, 3}, {6, 18}, {10.0, 14.2}, {12.6, 20.2}, {15.2, 19.0},
                    {12.7, 13.2}, {18, 12.6}};
        g.save(); g.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.setBrush(QColor(255, 255, 255, 210));
        g.drawPolygon(a); g.restore();
        return;
    }
    if (id == "select_window") {                 // 虚线矩形 + 加号（窗口选择）
        QRectF r(4.5, 4.5, 15, 15);
        QPen p(guide, 1.4); p.setStyle(Qt::DashLine); p.setDashPattern({3, 2});
        g.save(); g.setPen(p); g.drawRect(r); g.restore();
        g.save(); g.setPen(QPen(mark, 2.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(19.5, 8), QPointF(19.5, 14));
        g.drawLine(QPointF(16.5, 11), QPointF(22.5, 11));
        g.restore();
        return;
    }
    if (id == "select_all") {                    // 实线矩形 + 内部加号（全选）
        g.drawRect(QRectF(4.5, 4.5, 15, 15));
        g.save(); g.setPen(QPen(mark, 2.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(12, 8), QPointF(12, 16));
        g.drawLine(QPointF(8, 12), QPointF(16, 12));
        g.restore();
        return;
    }
    if (id == "deselect_all") {                  // 实线矩形 + 内部斜叉（全部不选）
        g.drawRect(QRectF(4.5, 4.5, 15, 15));
        g.save(); g.setPen(QPen(mark, 2.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(8.5, 8.5), QPointF(15.5, 15.5));
        g.drawLine(QPointF(15.5, 8.5), QPointF(8.5, 15.5));
        g.restore();
        return;
    }
    if (id == "deselect_window") {               // 虚线矩形 + 斜杠（窗口取消）
        QRectF r(4.5, 4.5, 15, 15);
        QPen p(guide, 1.4); p.setStyle(Qt::DashLine); p.setDashPattern({3, 2});
        g.save(); g.setPen(p); g.drawRect(r); g.restore();
        g.save(); g.setPen(QPen(mark, 2.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(6.5, 6.5), QPointF(17.5, 17.5)); g.restore();
        return;
    }
    if (id == "select_entity") {                 // 箭头指向实体点
        g.drawLine(QPointF(5, 18), QPointF(15, 8));
        arrow(15, 8, -M_PI / 4, mark);
        dot(18, 5, mark);
        return;
    }
    if (id == "deselect_entity") {               // 箭头指向实体点 + 斜杠（取消选中）
        g.drawLine(QPointF(5, 18), QPointF(15, 8));
        arrow(15, 8, -M_PI / 4, mark);
        dot(18, 5, mark);
        g.save(); g.setPen(QPen(mark, 1.8, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(15.5, 2.5), QPointF(20.5, 7.5)); g.restore();
        return;
    }
    if (id == "select_contour") {                // 相连轮廓链（强调连接）
        g.drawPolyline(QPolygonF{{4, 18}, {10, 8}, {15, 15}, {20, 6}});
        dot(4, 18, mark, 1.5); dot(10, 8, mark, 1.5);
        dot(15, 15, mark, 1.5); dot(20, 6, mark, 1.5);
        return;
    }
    if (id == "toggle_contour") {                // 相连轮廓链 + 环形双箭头（选/取消切换）
        g.drawPolyline(QPolygonF{{3, 20}, {8, 12}, {13, 17}, {18, 8}});
        dot(3, 20, mark, 1.5); dot(8, 12, mark, 1.5);
        dot(13, 17, mark, 1.5); dot(18, 8, mark, 1.5);
        g.save(); g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::RoundCap));
        g.drawArc(QRectF(14.5, 13.5, 7, 7), 30 * 16, 280 * 16);
        arrow(16.2, 13.2, -M_PI / 2.4, mark, 3.4);
        g.restore();
        return;
    }
    if (id == "select_intersected") {            // 两条相交线（X）
        g.drawLine(QPointF(5, 5), QPointF(19, 19));
        g.drawLine(QPointF(5, 19), QPointF(19, 5));
        dot(12, 12, mark, 2.0);
        return;
    }
    if (id == "fence") {                         // 栏选：折线栏线穿过图元
        g.save(); g.setPen(QPen(mark, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawPolyline(QPolygonF{{3, 17}, {9, 9}, {14, 15}, {21, 5}});
        g.restore();
        dot(3, 17, mark, 1.5); dot(9, 9, mark, 1.5);
        dot(14, 15, mark, 1.5); dot(21, 5, mark, 1.5);
        g.save(); g.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap));
        g.drawRect(QRectF(6.5, 11.5, 4, 4));
        g.drawRect(QRectF(13.5, 9.5, 4, 4));
        g.restore();
        return;
    }
    if (id == "wp") {                            // 圈围：实线多边形全包
        g.save(); g.setPen(QPen(mark, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawPolygon(QPolygonF{{12, 3}, {21, 10}, {17, 20}, {7, 20}, {3, 10}});
        g.restore();
        dot(12, 12, mark, 1.8);
        return;
    }
    if (id == "cp") {                            // 圈交：虚线多边形相交
        g.save(); g.setPen(QPen(mark, 1.7, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawPolygon(QPolygonF{{12, 3}, {21, 10}, {17, 20}, {7, 20}, {3, 10}});
        g.restore();
        g.save(); g.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(4, 16), QPointF(20, 8));
        g.restore();
        return;
    }
    if (id == "deselect_intersected") {          // 相交线 + 斜杠（取消相交）
        g.drawLine(QPointF(5, 5), QPointF(19, 19));
        g.drawLine(QPointF(5, 19), QPointF(19, 5));
        g.save(); g.setPen(QPen(mark, 1.8, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(8, 8), QPointF(16, 16)); g.restore();
        return;
    }
    if (id == "select_layer") {                  // 堆叠图层
        g.save(); g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawRect(QRectF(5, 5, 14, 4.5));
        g.drawRect(QRectF(5, 10.5, 14, 4.5));
        g.setBrush(QColor(0xD1, 0x4B, 0x3F, 60));
        g.drawRect(QRectF(5, 16, 14, 4.5));
        g.restore();
        return;
    }
    if (id == "toggle_layer") {                  // 堆叠图层 + 环形双箭头（选/取消切换）
        g.save(); g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawRect(QRectF(4, 3.5, 12, 4));
        g.drawRect(QRectF(4, 8.5, 12, 4));
        g.setBrush(QColor(0xD1, 0x4B, 0x3F, 60));
        g.drawRect(QRectF(4, 13.5, 12, 4));
        g.restore();
        g.save(); g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::RoundCap));
        g.drawArc(QRectF(14.5, 13.5, 7, 7), 30 * 16, 280 * 16);
        arrow(16.2, 13.2, -M_PI / 2.4, mark, 3.4);
        g.restore();
        return;
    }
    if (id == "invert_selection") {              // 反选：上下两条半环箭头对转
        g.save(); g.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap));
        g.drawArc(QRectF(4.5, 5.5, 15, 13), 0 * 16, 160 * 16);
        g.drawArc(QRectF(4.5, 5.5, 15, 13), 180 * 16, 160 * 16);
        g.restore();
        arrow(18.6, 8.2, -M_PI / 3.2, mark, 4.2);
        arrow(5.4, 15.8, M_PI - M_PI / 3.2, mark, 4.2);
        return;
    }
    if (id == "line") {                         // 带端点的斜线
        g.drawLine(QPointF(5, 19), QPointF(19, 5));
        dot(5, 19, mark); dot(19, 5, mark);
        return;
    }
    if (id == "xline") {                        // 构造线：两端出界的贯通斜线 + 通过点（无限延伸、无端点）
        g.drawLine(QPointF(2, 22), QPointF(22, 2));
        dot(12, 12, mark, 1.5);
        return;
    }
    if (id == "circle") {                       // 圆 + 圆心 + 半径虚线
        g.drawEllipse(QPointF(12.0, 12.0), 8.0, 8.0);
        dash(12, 12, 12 + 8 * 0.707, 12 - 8 * 0.707);
        dot(12, 12, mark, 1.5);
        return;
    }
    if (id == "circle3p") {                     // 圆 + 圆周上三个点
        g.drawEllipse(QPointF(12.0, 12.0), 8.0, 8.0);
        dot(12.0, 4.0, mark);
        dot(4.9, 16.0, mark);
        dot(19.1, 16.0, mark);
        return;
    }
    if (id == "arc") {                          // 圆弧 + 两端点
        QRectF r(4, 4, 16, 16);
        g.drawArc(r, 20 * 16, 140 * 16);
        dot(12 + 8 * std::cos(20 * M_PI / 180), 12 - 8 * std::sin(20 * M_PI / 180), mark);
        dot(12 + 8 * std::cos(160 * M_PI / 180), 12 - 8 * std::sin(160 * M_PI / 180), mark);
        return;
    }
    if (id == "ellipse") {                      // 椭圆 + 圆心 + 长短半径虚线
        g.drawEllipse(QPointF(12.0, 12.0), 9.0, 5.5);
        dash(12, 12, 21, 12);                   // 长半径
        dash(12, 12, 12, 6.5);                  // 短半径
        dot(12, 12, mark, 1.5);
        return;
    }
    if (id == "rect") {                         // 矩形 + 对角点
        g.drawRect(QRectF(4.5, 6.5, 15, 11));
        dot(4.5, 17.5, mark); dot(19.5, 6.5, mark);
        return;
    }
    if (id == "polyline") {                     // 折线：折线段 + 节点
        g.drawLine(QPointF(4.0, 16.0), QPointF(11.0, 8.0));
        g.drawLine(QPointF(11.0, 8.0), QPointF(15.0, 14.0));
        g.drawLine(QPointF(15.0, 14.0), QPointF(20.0, 6.0));
        dot(4.0, 16.0, mark); dot(11.0, 8.0, mark);
        dot(15.0, 14.0, mark); dot(20.0, 6.0, mark);
        return;
    }
    if (id == "poly") {                         // 正五边形
        QPolygonF p;
        for (int i = 0; i < 5; ++i) {
            double a = -M_PI / 2 + i * 2 * M_PI / 5;
            p << QPointF(12 + 8.4 * std::cos(a), 12 + 8.4 * std::sin(a));
        }
        g.drawPolygon(p);
        return;
    }
    if (id == "spline") {                       // 平滑 S 曲线 + 控制点
        QPainterPath p(QPointF(4, 18));
        p.cubicTo(QPointF(8, 4), QPointF(16, 20), QPointF(20, 6));
        g.drawPath(p);
        dot(4, 18, mark, 1.5); dot(20, 6, mark, 1.5);
        dot(12, 12.4, guide, 1.4);
        return;
    }
    if (id == "dim") {                          // 尺寸标注：双向箭头 + 界线
        g.save(); g.setPen(QPen(guide, 1.2));
        g.drawLine(QPointF(5, 5), QPointF(5, 19));
        g.drawLine(QPointF(19, 5), QPointF(19, 19));
        g.restore();
        g.drawLine(QPointF(5, 14), QPointF(19, 14));
        arrow(5, 14, M_PI, ink);
        arrow(19, 14, 0, ink);
        text("L", mark, 8.0, QRectF(0, 1, 24, 10));
        return;
    }
    if (id == "constraint") {                   // 几何约束：两条平行线 + 等号标记
        g.drawLine(QPointF(4, 8), QPointF(20, 8));
        g.drawLine(QPointF(4, 16), QPointF(20, 16));
        g.save(); g.setPen(QPen(mark, 1.5, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(10, 11), QPointF(14, 11));
        g.drawLine(QPointF(10, 13), QPointF(14, 13));
        g.restore();
        return;
    }
    if (id == "trim") {                         // 修剪：实线 + 被剪掉的虚线段 + 剪切叉
        g.drawLine(QPointF(3, 16), QPointF(11, 16));
        dash(11, 16, 21, 16);
        g.save(); g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(8, 5), QPointF(14, 11));
        g.drawLine(QPointF(14, 5), QPointF(8, 11));
        g.restore();
        return;
    }
    if (id == "extend") {                       // 延伸：实线 + 虚线 + 前向箭头
        g.drawLine(QPointF(3, 14), QPointF(12, 14));
        dash(12, 14, 18, 14);
        arrow(20, 14, 0, mark, 5.0);
        g.save(); g.setPen(QPen(guide, 1.2));
        g.drawLine(QPointF(20.5, 6), QPointF(20.5, 20));
        g.restore();
        return;
    }
    if (id == "modify") {                       // 修改（总入口）：铅笔 + 旋转箭头
        g.save();
        g.setPen(QPen(ink, 1.6));
        g.drawLine(QPointF(4, 19), QPointF(12, 11));    // 笔杆
        g.drawLine(QPointF(12, 11), QPointF(9, 8));     // 笔尖
        g.drawLine(QPointF(12, 11), QPointF(15, 8));
        g.restore();
        g.save(); g.setPen(QPen(mark, 1.5));
        g.drawArc(QRectF(11, 4, 13, 13), 30 * 16, 300 * 16);   // 旋转弧
        g.restore();
        arrow(23, 14, -60, mark, 4.0);
        return;
    }
    // ================= 修改菜单图标（Modify Menu）=================
    if (id == "move_copy") {                    // 移动/复制：箭头 + 复件虚影
        g.drawLine(QPointF(4, 8), QPointF(13, 8));
        arrow(15, 8, 0, ink, 4.5);
        g.save(); g.setPen(QPen(guide, 1.0, Qt::DashLine));
        g.drawRect(QRectF(11, 12, 9, 7));
        g.restore();
        return;
    }
    if (id == "duplicate") {                    // 复制：原矩形 + 偏置矩形
        g.drawRect(QRectF(4, 6, 10, 8));
        g.save(); g.setPen(QPen(mark, 1.4));
        g.drawRect(QRectF(10, 12, 10, 8)); g.restore();
        return;
    }
    if (id == "align") {                        // 对齐：三条横线左对齐
        g.save(); g.setPen(QPen(guide, 1.0, Qt::DashLine));
        g.drawLine(QPointF(8, 6), QPointF(20, 6));
        g.drawLine(QPointF(8, 12), QPointF(20, 12));
        g.drawLine(QPointF(8, 18), QPointF(20, 18));
        g.restore();
        g.drawLine(QPointF(8, 9), QPointF(17, 9));
        g.drawLine(QPointF(8, 15), QPointF(15, 15));
        return;
    }
    if (id == "align_one") {                    // 对齐单条：箭头指向参考线
        g.save(); g.setPen(QPen(guide, 1.2, Qt::DashLine));
        g.drawLine(QPointF(3, 18), QPointF(21, 18));
        g.restore();
        g.drawLine(QPointF(3, 9), QPointF(14, 9));
        arrow(15, 9, 0, ink, 4.0);
        return;
    }
    if (id == "align_ref") {                    // 参考点对齐：圆心 + 十字标
        g.drawEllipse(QPointF(8, 12), 5, 5);
        g.save(); g.setPen(QPen(mark, 1.4));
        g.drawLine(QPointF(8, 4), QPointF(8, 9));
        g.drawLine(QPointF(4, 12), QPointF(13, 12));
        g.drawEllipse(QPointF(17, 14), 3.5, 3.5);
        g.restore();
        return;
    }
    if (id == "rotate") {                       // 旋转：箭头绕圆心旋转
        g.drawEllipse(QPointF(12, 12), 6.5, 6.5);
        g.save(); g.setPen(QPen(mark, 1.6));
        g.drawLine(QPointF(12, 12), QPointF(12, 5.5));
        arrow(12, 5.5, -M_PI / 2, mark, 3.5);
        g.restore();
        g.save(); QPen arc(ink, 1.4); arc.setStyle(Qt::DashLine);
        g.setPen(arc);
        g.drawArc(QRectF(3, 3, 18, 18), 30 * 16, 240 * 16);
        g.restore();
        return;
    }
    if (id == "scale") {                        // 缩放：方框 + 角手柄 + 双箭头
        g.drawRect(QRectF(5, 5, 14, 14));
        g.save(); g.setPen(QPen(mark, 1.4));
        g.drawLine(QPointF(7, 7), QPointF(17, 17));
        arrow(7, 7, M_PI / 4, mark, 3.5);
        arrow(17, 17, M_PI / 4 + M_PI, mark, 3.5);
        g.restore();
        return;
    }
    if (id == "mirror") {                       // 镜像：水平线 + 半圆弧 + 上下对称对象
        g.save(); QPen p(guide, 1.0); p.setStyle(Qt::DashLine);
        g.setPen(p); g.drawLine(QPointF(12, 2), QPointF(12, 22));
        g.restore();
        g.drawLine(QPointF(5, 9), QPointF(11, 9));
        arrow(11, 9, 0, ink, 4.0);
        g.drawLine(QPointF(19, 15), QPointF(13, 15));
        arrow(13, 15, M_PI, ink, 4.0);
        return;
    }
    if (id == "move_rotate") {                  // 移动+旋转：箭头 + 圆弧
        g.drawLine(QPointF(4, 8), QPointF(11, 8));
        arrow(13, 8, 0, ink, 4.0);
        g.drawEllipse(QPointF(15, 16), 5, 5);
        g.save(); g.setPen(QPen(mark, 1.4));
        g.drawLine(QPointF(15, 16), QPointF(18, 12));
        arrow(18.5, 11.5, -M_PI / 4, mark, 3.5);
        g.restore();
        return;
    }
    if (id == "rotate_two") {                   // 旋转两图元：两条线 + 旋转
        g.drawLine(QPointF(3, 6), QPointF(15, 6));
        g.drawLine(QPointF(3, 18), QPointF(15, 18));
        g.save(); QPen arc(ink, 1.4); arc.setStyle(Qt::DashLine);
        g.setPen(arc);
        g.drawArc(QRectF(11, 8, 10, 10), 30 * 16, 270 * 16);
        g.restore();
        return;
    }
    if (id == "revert_direction") {             // 反向：箭头 180° 翻转
        g.drawLine(QPointF(5, 12), QPointF(15, 12));
        arrow(5, 12, M_PI, ink, 4.0);
        arrow(15, 12, 0, mark, 4.0);
        return;
    }
    if (id == "trim_two") {                     // 修剪二：两条相交线 + 叉号
        g.drawLine(QPointF(3, 21), QPointF(21, 3));
        g.drawLine(QPointF(3, 3), QPointF(21, 21));
        g.save(); g.setPen(QPen(mark, 1.6));
        g.drawLine(QPointF(10, 10), QPointF(14, 14));
        g.drawLine(QPointF(14, 10), QPointF(10, 14));
        g.restore();
        return;
    }
    if (id == "lengthen") {                     // 延长：实线 + 虚线延伸 + 前向箭头
        g.drawLine(QPointF(3, 12), QPointF(11, 12));
        dash(11, 12, 16, 12);
        arrow(18, 12, 0, mark, 4.5);
        return;
    }
    if (id == "line_join") {                    // 连接两线段
        g.save(); QPen p(guide, 1.0); p.setStyle(Qt::DashLine);
        g.setPen(p);
        g.drawLine(QPointF(5, 7), QPointF(13, 7));
        g.drawLine(QPointF(13, 7), QPointF(13, 19));
        g.restore();
        g.drawLine(QPointF(5, 7), QPointF(13, 7));
        g.drawLine(QPointF(13, 7), QPointF(13, 19));
        g.save(); g.setPen(QPen(mark, 1.6));
        g.drawLine(QPointF(11, 5), QPointF(11, 9));
        g.drawLine(QPointF(9, 7), QPointF(13, 7));
        g.restore();
        return;
    }
    if (id == "break_divide") {                 // 打断/等分：直线 + 节点标记
        g.drawLine(QPointF(3, 12), QPointF(21, 12));
        g.save(); g.setPen(Qt::NoPen); g.setBrush(mark);
        g.drawEllipse(QPointF(9, 12), 1.8, 1.8);
        g.drawEllipse(QPointF(15, 12), 1.8, 1.8);
        g.restore();
        g.save(); QPen sc(ink, 1.4); sc.setStyle(Qt::DashLine);
        g.setPen(sc);
        g.drawLine(QPointF(9, 7), QPointF(9, 17));
        g.drawLine(QPointF(15, 7), QPointF(15, 17));
        g.restore();
        return;
    }
    if (id == "line_gap") {                     // 线间隙：两段断开的直线
        g.drawLine(QPointF(3, 12), QPointF(10, 12));
        g.drawLine(QPointF(14, 12), QPointF(21, 12));
        g.save(); QPen p(guide, 1.2); g.setPen(p);
        g.drawLine(QPointF(11, 9), QPointF(13, 15));
        g.restore();
        return;
    }
    if (id == "offset") {                       // 偏移：两条平行虚线 + 上下移动箭头
        g.drawLine(QPointF(3, 10), QPointF(21, 10));
        dash(3, 14, 21, 14);
        g.save(); g.setPen(QPen(mark, 1.5));
        arrow(12, 8, -M_PI / 2, mark, 4.0);
        arrow(12, 16, M_PI / 2, mark, 4.0);
        g.restore();
        return;
    }
    if (id == "bevel") {                        // 倒角：L 形 + 斜边
        g.drawLine(QPointF(5, 5), QPointF(5, 14));
        g.drawLine(QPointF(5, 14), QPointF(14, 14));
        g.save(); g.setPen(QPen(mark, 1.8));
        g.drawLine(QPointF(5, 9), QPointF(9, 14));
        g.restore();
        return;
    }
    if (id == "fillet") {                       // 圆角：L 形 + 圆弧
        g.drawLine(QPointF(5, 5), QPointF(5, 13));
        g.drawLine(QPointF(5, 13), QPointF(13, 13));
        g.save(); g.setPen(QPen(mark, 1.8));
        g.drawArc(QRectF(4, 8, 6, 6), 180 * 16, 90 * 16);
        g.restore();
        return;
    }
    if (id == "divide") {                       // 等分：直线 + 等分点
        g.drawLine(QPointF(3, 12), QPointF(21, 12));
        g.save(); g.setPen(Qt::NoPen); g.setBrush(ink);
        for (int k = 0; k <= 4; ++k)
            g.drawEllipse(QPointF(3 + 4.5 * k, 12), 1.3, 1.3);
        g.restore();
        return;
    }
    if (id == "stretch") {                      // 拉伸：方框 + 单边延长箭头
        g.drawRect(QRectF(4, 6, 11, 12));
        g.save(); g.setPen(QPen(mark, 1.5));
        g.drawLine(QPointF(15, 12), QPointF(21, 12));
        arrow(21, 12, 0, mark, 4.0);
        g.restore();
        return;
    }
    if (id == "properties") {                   // 属性：齿轮
        g.save(); g.setBrush(Qt::NoBrush); g.setPen(QPen(ink, 1.4));
        g.drawEllipse(QPointF(12, 12), 7, 7);
        g.setPen(QPen(mark, 1.4));
        g.drawEllipse(QPointF(12, 12), 3, 3);
        for (int k = 0; k < 8; ++k) {
            const double ang = k * M_PI / 4;
            const double x1 = 12 + 6 * std::cos(ang), y1 = 12 + 6 * std::sin(ang);
            const double x2 = 12 + 8 * std::cos(ang), y2 = 12 + 8 * std::sin(ang);
            g.setPen(QPen(ink, 1.4));
            g.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        }
        g.restore();
        return;
    }
    if (id == "attributes") {                   // 属性：色块 + 标签
        g.drawRect(QRectF(3, 5, 18, 7));
        g.save(); g.setPen(QPen(guide, 1.0));
        g.drawLine(QPointF(3, 16), QPointF(21, 16));
        g.drawLine(QPointF(3, 19), QPointF(15, 19));
        g.restore();
        return;
    }
    if (id == "explode") {                      // 分解：实心方块 + 飞出四箭头
        g.drawRect(QRectF(8, 8, 8, 8));
        g.save(); g.setPen(QPen(mark, 1.5));
        arrow(12, 12, 0,           mark, 4.0);
        arrow(12, 12, M_PI / 2,    mark, 4.0);
        arrow(12, 12, M_PI,        mark, 4.0);
        arrow(12, 12, -M_PI / 2,   mark, 4.0);
        g.restore();
        return;
    }
    if (id == "explode_text") {                 // 文本分解：T + 飞出箭头
        g.save(); g.setFont(QFont("Arial", 14, QFont::Bold));
        g.setPen(ink);
        g.drawText(QRectF(3, 4, 14, 16), Qt::AlignCenter, QStringLiteral("T"));
        g.restore();
        g.save(); g.setPen(QPen(mark, 1.4));
        arrow(15, 12, 0, mark, 4.0);
        arrow(18, 12, M_PI / 2, mark, 4.0);
        g.restore();
        return;
    }
    // ================= 编辑 / 文件 =================
    if (id == "delete") {                       // 垃圾桶
        g.drawLine(QPointF(4.5, 7), QPointF(19.5, 7));
        g.drawRect(QRectF(6.5, 7, 11, 12.5));
        g.drawLine(QPointF(9.5, 4.5), QPointF(14.5, 4.5));
        g.save(); g.setPen(QPen(mark, 1.3));
        g.drawLine(QPointF(10, 10), QPointF(10, 16.5));
        g.drawLine(QPointF(14, 10), QPointF(14, 16.5));
        g.restore();
        return;
    }
    if (id == "undo" || id == "redo") {
        const bool re = (id == "redo");
        QRectF r(5, 7, 14, 12);
        g.drawArc(r, re ? 30 * 16 : 30 * 16, 120 * 16);
        if (re) arrow(18.2, 9.6, -0.6, ink);
        else    arrow(5.8, 9.6, M_PI + 0.6, ink);
        g.drawLine(QPointF(12, 13), QPointF(12, 19));
        return;
    }
    if (id == "copy") {
        g.save(); g.setPen(QPen(guide, 1.4));
        g.drawRect(QRectF(4.5, 4.5, 11, 12)); g.restore();
        g.setBrush(QColor(255, 255, 255, 220));
        g.drawRect(QRectF(8.5, 7.5, 11, 12));
        return;
    }
    if (id == "paste") {
        g.drawRect(QRectF(5, 6, 14, 13.5));
        g.save(); g.setPen(Qt::NoPen); g.setBrush(mark);
        g.drawRect(QRectF(8.5, 3.5, 7, 4)); g.restore();
        g.save(); g.setPen(QPen(guide, 1.2));
        g.drawLine(QPointF(8, 12), QPointF(16, 12));
        g.drawLine(QPointF(8, 15.5), QPointF(16, 15.5));
        g.restore();
        return;
    }
    if (id == "new") {
        QPolygonF p{{6, 3.5}, {14.5, 3.5}, {18.5, 7.5}, {18.5, 20.5}, {6, 20.5}};
        g.drawPolygon(p);
        g.save(); g.setPen(QPen(guide, 1.2));
        g.drawPolyline(QPolygonF{{14.5, 3.5}, {14.5, 7.5}, {18.5, 7.5}}); g.restore();
        return;
    }
    if (id == "open") {
        g.drawPolyline(QPolygonF{{3.5, 19}, {3.5, 6}, {9.5, 6}, {11.5, 8.5}, {18, 8.5}, {18, 11}});
        g.save(); g.setPen(QPen(ink, 1.5)); g.setBrush(QColor(255, 255, 255, 160));
        g.drawPolygon(QPolygonF{{3.5, 19}, {7.5, 11}, {21.5, 11}, {17.5, 19}}); g.restore();
        return;
    }
    if (id == "save") {
        g.drawRect(QRectF(4.5, 4.5, 15, 15));
        g.save(); g.setPen(Qt::NoPen); g.setBrush(ink);
        g.drawRect(QRectF(8, 4.5, 8, 5)); g.restore();
        g.save(); g.setPen(QPen(guide, 1.3));
        g.drawRect(QRectF(8, 13, 8, 6.5)); g.restore();
        return;
    }
    if (id == "export") {
        g.drawPolyline(QPolygonF{{14, 4.5}, {19.5, 4.5}, {19.5, 19.5}, {14, 19.5}});
        g.drawLine(QPointF(4, 12), QPointF(14.5, 12));
        arrow(16, 12, 0, mark, 5.0);
        return;
    }

    // ================= 视图 =================
    if (id == "grid") {
        g.save(); g.setPen(QPen(guide, 1.1));
        for (int i = 1; i <= 3; ++i) {
            g.drawLine(QPointF(4, 4 + i * 4.0), QPointF(20, 4 + i * 4.0));
            g.drawLine(QPointF(4 + i * 4.0, 4), QPointF(4 + i * 4.0, 20));
        }
        g.restore();
        g.drawRect(QRectF(4, 4, 16, 16));
        return;
    }
    if (id == "zoomin")  { magnifier(true,  false); return; }
    if (id == "zoomout") { magnifier(false, false); return; }
    if (id == "fit") {
        g.save(); g.setPen(QPen(guide, 1.3));
        g.drawRect(QRectF(3.5, 5.5, 17, 13)); g.restore();
        g.save(); g.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap));
        // 四角折线
        g.drawPolyline(QPolygonF{{7, 9}, {7, 12}});   g.drawPolyline(QPolygonF{{7, 9}, {10, 9}});
        g.drawPolyline(QPolygonF{{17, 9}, {17, 12}}); g.drawPolyline(QPolygonF{{17, 9}, {14, 9}});
        g.drawPolyline(QPolygonF{{7, 15}, {7, 12}});  g.drawPolyline(QPolygonF{{7, 15}, {10, 15}});
        g.drawPolyline(QPolygonF{{17, 15}, {17, 12}});g.drawPolyline(QPolygonF{{17, 15}, {14, 15}});
        g.restore();
        return;
    }
    if (id == "zoombox") {                      // 窗口缩放：虚线框 + 四角折线 + 放大镜角标
        QPen p(guide, 1.3); p.setStyle(Qt::DashLine); p.setDashPattern({3, 2});
        g.save(); g.setPen(p);
        g.drawRect(QRectF(3.5, 5.5, 17, 13)); g.restore();
        // 四角折线（强调"框选"）
        g.save(); g.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap));
        g.drawPolyline(QPolygonF{{7, 9}, {7, 12}});   g.drawPolyline(QPolygonF{{7, 9}, {10, 9}});
        g.drawPolyline(QPolygonF{{17, 9}, {17, 12}}); g.drawPolyline(QPolygonF{{17, 9}, {14, 9}});
        g.drawPolyline(QPolygonF{{7, 15}, {7, 12}});  g.drawPolyline(QPolygonF{{7, 15}, {10, 15}});
        g.drawPolyline(QPolygonF{{17, 15}, {17, 12}});g.drawPolyline(QPolygonF{{17, 15}, {14, 15}});
        g.restore();
        // 放大镜角标（右下）：表示该区域将被放大充满
        g.save(); g.setPen(QPen(mark, 1.4));
        g.drawEllipse(QPointF(16.5, 16.5), 3.2, 3.2);
        g.drawLine(QPointF(18.8, 18.8), QPointF(21.5, 21.5));
        g.restore();
        return;
    }
    if (id == "plane_xy" || id == "plane_yz" || id == "plane_xz") {
        // 坐标轴象限图：Y 向上、X 向右（与视图实际方向一致，起提示作用）
        g.save(); g.setPen(QPen(guide, 1.3));
        g.drawLine(QPointF(5, 19), QPointF(21, 19));   // 横轴
        g.drawLine(QPointF(5, 19), QPointF(5, 3));     // 纵轴
        g.restore();
        arrow(21.5, 19, 0, guide, 3.6);
        arrow(5, 2.5, -M_PI / 2, guide, 3.6);
        QString t = id.right(2).toUpper();
        text(t, ink, 9.0, QRectF(6, 3, 16, 14));
        return;
    }

    // ================= 样式 =================
    if (id == "color") {                        // 调色盘
        g.save();
        QPainterPath p;
        p.moveTo(12, 3.2);
        p.arcTo(QRectF(3, 3, 18, 18), 90, 300);
        p.lineTo(15.5, 15.5);
        p.closeSubpath();
        g.setPen(QPen(ink, 1.4)); g.setBrush(Qt::NoBrush);
        g.drawEllipse(QPointF(12.0, 12.0), 9.0, 9.0);
        g.setPen(Qt::NoPen);
        g.setBrush(QColor(0xD1, 0x4B, 0x3F)); g.drawEllipse(QPointF(9.0, 7.5), 2.0, 2.0);
        g.setBrush(QColor(0x2B, 0x59, 0xA8)); g.drawEllipse(QPointF(6.4, 12.5), 2.0, 2.0);
        g.setBrush(QColor(0x2E, 0x8B, 0x57)); g.drawEllipse(QPointF(9.6, 16.8), 2.0, 2.0);
        g.setBrush(QColor(0xE6, 0x91, 0x38)); g.drawEllipse(QPointF(15.2, 8.6), 2.0, 2.0);
        g.restore();
        return;
    }
    if (id == "width") {                        // 线宽：三条不同粗细的横线
        g.save(); g.setPen(QPen(ink, 1.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(4, 7), QPointF(20, 7));
        g.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(4, 12), QPointF(20, 12));
        g.setPen(QPen(ink, 4.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(4, 17.5), QPointF(20, 17.5));
        g.restore();
        return;
    }
    if (id == "chain") {                        // 连续直线：折线链 + 节点
        g.drawPolyline(QPolygonF{{4, 17}, {10, 7}, {15, 15}, {20, 6}});
        dot(4, 17, mark, 1.5); dot(10, 7, mark, 1.5);
        dot(15, 15, mark, 1.5); dot(20, 6, mark, 1.5);
        return;
    }
    if (id == "help") {
        g.drawEllipse(QPointF(12.0, 12.0), 8.5, 8.5);
        text("?", mark, 12.0, QRectF(0, 0, 24, 24));
        return;
    }
    if (id == "hatch") {                        // 剖面填充：方框内 45° 剖面线
        const QRectF box(4.5, 4.5, 15, 15);
        g.save();
        g.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        g.drawRect(box);
        QPainterPath clip; clip.addRect(box);
        g.setClipPath(clip);
        g.setPen(QPen(ink, 1.0, Qt::SolidLine, Qt::FlatCap));
        for (double c = -box.height(); c <= box.width(); c += 3.2)
            g.drawLine(QPointF(box.left() + c, box.bottom()),
                       QPointF(box.left() + c + box.height(), box.top()));
        g.restore();
        return;
    }
    if (id == "boundary" || id == "bpoly") {    // 边界/边界多边形：虚线方框（自定义填充边界）
        const QRectF box(4.5, 4.5, 15, 15);
        g.save();
        QPen dp(ink, 1.6, Qt::DashLine, Qt::SquareCap);   // 虚线：与屏幕渲染的边界轮廓一致
        dp.setDashPattern({3.0, 2.0});
        g.setPen(dp);
        g.setBrush(Qt::NoBrush);
        g.drawRect(box);
        // 一个内孔（小虚线框），暗示边界可带内孔
        g.drawRect(QRectF(10.5, 10.5, 5, 5));
        g.restore();
        return;
    }

    if (id == "cleanup") {                      // 图形清理：扫帚（清扫垃圾/冗余图元）
        // 扫帚柄
        g.drawLine(QPointF(6, 6), QPointF(13, 13));
        // 扫帚刷毛（自柄端向右下扇形展开）
        g.drawLine(QPointF(13, 13), QPointF(19.5, 14));
        g.drawLine(QPointF(13, 13), QPointF(20, 16));
        g.drawLine(QPointF(13, 13), QPointF(19, 18));
        // 地面虚线（被清扫的基准，暗示"扫除"动作）
        QPen gp(guide, 1.2); gp.setStyle(Qt::DashLine); gp.setDashPattern({3, 2});
        g.save(); g.setPen(gp);
        g.drawLine(QPointF(12, 19), QPointF(21, 19));
        g.restore();
        return;
    }

    if (id == "block") {                        // 块：外框方块 + 内部小方块（定义 + 实例）
        g.drawRect(QRectF(3.5, 3.5, 17, 17));
        g.drawRect(QRectF(10.0, 10.0, 7, 7));
        return;
    }
    if (id == "query") {                        // 测量查询：放大镜 + 镜内尺寸双向箭头
        // 放大镜主体（镜框 + 手柄）
        g.drawEllipse(QPointF(9.5, 9.5), 6.0, 6.0);
        g.drawLine(QPointF(13.7, 13.7), QPointF(20.0, 20.0));
        // 镜内尺寸线：双向箭头（量距），强调"量测"语义
        QPen dp(mark, 1.6);
        g.save(); g.setPen(dp);
        g.drawLine(QPointF(5.5, 11.0), QPointF(13.5, 11.0));
        arrow(13.5, 11.0, 0.0, mark, 3.0);          // 右向箭头
        arrow(5.5, 11.0, 3.14159265, mark, 3.0);     // 左向箭头
        g.restore();
        return;
    }

    if (id == "print") {                        // 打印：打印机机身 + 纸张 + 出纸口
        // 机身
        g.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::SquareCap));
        g.drawRect(QRectF(4.5, 9.0, 15.0, 9.0));
        // 出纸托盘（底部两条横线）
        g.drawLine(QPointF(7.5, 19.5), QPointF(16.5, 19.5));
        g.drawLine(QPointF(9.0, 21.5), QPointF(15.0, 21.5));
        // 进纸（顶部纸张）
        g.drawRect(QRectF(8.0, 3.5, 8.0, 6.0));
        // 状态指示灯
        dot(17.0, 11.5, mark, 1.3);
        return;
    }

    if (id == "mtext") {                       // 多行文字：文字框 + 多行横线 + 大"A"
        g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        g.drawRect(QRectF(3.5, 4.0, 17, 16));          // 文本框
        g.setPen(QPen(guide, 1.2, Qt::SolidLine, Qt::SquareCap));
        g.drawLine(QPointF(6, 9),  QPointF(18, 9));    // 第 1 行
        g.drawLine(QPointF(6, 13), QPointF(15, 13));   // 第 2 行
        g.drawLine(QPointF(6, 17), QPointF(13, 17));   // 第 3 行
        g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::SquareCap));
        QFont fa = g.font(); fa.setPixelSize(9); fa.setBold(true); g.setFont(fa);
        g.drawText(QRectF(3.5, 4.0, 17, 16), Qt::AlignCenter, QStringLiteral("A"));
        return;
    }
    if (id == "style") {                        // 文字样式：大"A" + 下划横线（字体图标）
        QFont fa = g.font(); fa.setPixelSize(15); fa.setBold(true); g.setFont(fa);
        g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.drawText(QRectF(3, 2, 18, 16), Qt::AlignCenter, QStringLiteral("A"));
        g.setPen(QPen(mark, 1.6));
        g.drawLine(QPointF(6, 19), QPointF(18, 19));    // 样式基线
        return;
    }
    if (id == "field") {                        // 字段：带百分号的标签牌
        g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        g.drawRoundedRect(QRectF(4, 7, 16, 10), 2.5, 2.5);   // 标签牌
        g.drawLine(QPointF(4, 12), QPointF(20, 12));         // 牌中线
        g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::SquareCap));
        QFont ff = g.font(); ff.setPixelSize(10); ff.setBold(true); g.setFont(ff);
        g.drawText(QRectF(4, 7, 16, 10), Qt::AlignCenter, QStringLiteral("%"));
        return;
    }
    if (id == "table") {                        // 表格：外框 + 内部网格线 + 高亮首行
        g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        g.drawRect(QRectF(3.5, 5.0, 17, 14));                // 表格外框
        g.setPen(QPen(guide, 1.1, Qt::SolidLine, Qt::SquareCap));
        g.drawLine(QPointF(3.5, 10),  QPointF(20.5, 10));    // 行线 1
        g.drawLine(QPointF(3.5, 14),  QPointF(20.5, 14));    // 行线 2
        g.drawLine(QPointF(9.0, 5.0), QPointF(9.0, 19));     // 列线 1
        g.drawLine(QPointF(15.0, 5.0), QPointF(15.0, 19));   // 列线 2
        g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::SquareCap));
        g.drawLine(QPointF(5, 7.5),  QPointF(19, 7.5));      // 首行强调线（表头）
        return;
    }
    if (id == "group") {                        // 编组：虚线框圈住三个小方块
        g.setPen(QPen(ink, 1.3, Qt::DashLine));  // 虚线 = 编组边界
        g.setBrush(Qt::NoBrush);
        g.drawRect(QRectF(3.5, 4.5, 17, 15));
        g.setPen(QPen(mark, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        g.drawRect(QRectF(7, 8, 4, 4));          // 成员对象 1
        g.drawRect(QRectF(13, 8, 4, 4));         // 成员对象 2
        g.drawRect(QRectF(10, 13, 4, 4));        // 成员对象 3
        return;
    }
    if (id == "qselect") {                      // 快速选择：漏斗 + 闪电（快速）
        g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        // 漏斗形
        QPainterPath funnel;
        funnel.moveTo(3.5, 4.5);  funnel.lineTo(20.5, 4.5);
        funnel.lineTo(14.0, 12.0); funnel.lineTo(14.0, 19.5);
        funnel.lineTo(10.0, 17.0); funnel.lineTo(10.0, 12.0);
        funnel.closeSubpath();
        g.drawPath(funnel);
        g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(17, 14), QPointF(19.5, 16.5));   // 强调斜线
        g.drawLine(QPointF(19.5, 14), QPointF(17, 16.5));
        return;
    }
    if (id == "filter") {                       // 过滤器：漏斗 + 内部条件行
        g.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::SquareCap));
        g.setBrush(Qt::NoBrush);
        QPainterPath f2;
        f2.moveTo(3.5, 4.5);  f2.lineTo(20.5, 4.5);
        f2.lineTo(13.5, 12.5); f2.lineTo(13.5, 19.5);
        f2.lineTo(9.5, 16.5);  f2.lineTo(9.5, 12.5);
        f2.closeSubpath();
        g.drawPath(f2);
        g.setPen(QPen(guide, 1.1, Qt::SolidLine, Qt::SquareCap));
        g.drawLine(QPointF(6.5, 7.0),  QPointF(17.5, 7.0));  // 条件行 1
        g.drawLine(QPointF(8.5, 9.5),  QPointF(14.5, 9.5));  // 条件行 2
        return;
    }

    // ================= 菜单补齐（顶级菜单/菜单项此前缺失或 id 未定义） =================
    if (id == "script") {                       // 脚本：文档 + 代码尖括号
        g.drawRoundedRect(QRectF(4.5, 3.5, 15, 17), 2.0, 2.0);
        g.save();
        g.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawLine(QPointF(10.0, 9.0),  QPointF(7.0, 12.5));   // <
        g.drawLine(QPointF(7.0, 12.5),  QPointF(10.0, 16.0));
        g.drawLine(QPointF(14.0, 9.0),  QPointF(17.0, 12.5));  // >
        g.drawLine(QPointF(17.0, 12.5), QPointF(14.0, 16.0));
        g.restore();
        return;
    }
    if (id == "track") {                        // 追踪：虚线十字 + 极轴角度弧与方向箭头
        dash(12, 3.0, 12, 21.0);
        dash(3.0, 12, 21.0, 12);
        g.save();
        g.setPen(QPen(mark, 1.5, Qt::SolidLine, Qt::RoundCap));
        g.drawArc(QRectF(6.0, 6.0, 12.0, 12.0), 0, -45 * 16);   // 极轴角弧
        g.restore();
        arrow(18.0, 6.0, -0.7854, mark);                       // 追踪方向
        dot(12, 12, mark, 1.5);
        return;
    }
    if (id == "titleblock") {                   // 图框与标题栏：图纸 + 右下角图签格
        g.drawRect(QRectF(3.5, 4.5, 17, 15));
        g.save();
        g.setPen(QPen(mark, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawRect(QRectF(11.0, 14.0, 9.5, 5.5));
        g.drawLine(QPointF(11.0, 16.7), QPointF(20.5, 16.7));
        g.drawLine(QPointF(15.8, 14.0), QPointF(15.8, 19.5));
        g.restore();
        return;
    }
    if (id == "limits") {                       // 图纸界限：虚线范围框 + 四角实心角标
        QPen p(guide, 1.4); p.setStyle(Qt::DashLine); p.setDashPattern({3, 2});
        g.save(); g.setPen(p); g.drawRect(QRectF(4.5, 5.5, 15, 13)); g.restore();
        g.save();
        g.setPen(QPen(mark, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawLine(QPointF(4.5, 9.0),   QPointF(4.5, 5.5));  g.drawLine(QPointF(4.5, 5.5),   QPointF(8.0, 5.5));
        g.drawLine(QPointF(16.0, 5.5),  QPointF(19.5, 5.5)); g.drawLine(QPointF(19.5, 5.5),  QPointF(19.5, 9.0));
        g.drawLine(QPointF(4.5, 15.0),  QPointF(4.5, 18.5)); g.drawLine(QPointF(4.5, 18.5),  QPointF(8.0, 18.5));
        g.drawLine(QPointF(16.0, 18.5), QPointF(19.5, 18.5));g.drawLine(QPointF(19.5, 18.5), QPointF(19.5, 15.0));
        g.restore();
        return;
    }
    if (id == "snap") {                         // 捕捉：磁铁（栅格/对象吸附）
        g.save();
        g.setPen(QPen(mark, 3.0, Qt::SolidLine, Qt::RoundCap));
        g.drawArc(QRectF(7.0, 5.0, 10.0, 10.0), 180 * 16, 180 * 16);   // 马蹄半圆
        g.drawLine(QPointF(7.0, 10.0),  QPointF(7.0, 16.5));
        g.drawLine(QPointF(17.0, 10.0), QPointF(17.0, 16.5));
        g.restore();
        g.save();
        g.setPen(QPen(ink, 3.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(7.0, 17.5),  QPointF(7.0, 20.0));   // 磁极
        g.drawLine(QPointF(17.0, 17.5), QPointF(17.0, 20.0));
        g.restore();
        return;
    }
    if (id == "panel") {                        // 面板/工作空间：窗口 + 左侧停靠栏
        g.drawRect(QRectF(3.5, 4.5, 17, 15));
        g.save();
        g.setPen(QPen(mark, 1.5, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(9.0, 4.5), QPointF(9.0, 19.5));
        g.drawLine(QPointF(11.5, 8.5),  QPointF(18.5, 8.5));
        g.drawLine(QPointF(11.5, 12.0), QPointF(18.5, 12.0));
        g.drawLine(QPointF(11.5, 15.5), QPointF(16.0, 15.5));
        g.restore();
        return;
    }
    if (id == "regen") {                        // 重生成：环形刷新箭头
        g.save();
        g.setPen(QPen(ink, 1.7, Qt::SolidLine, Qt::RoundCap));
        g.drawArc(QRectF(5.5, 5.5, 13.0, 13.0), -70 * 16, 300 * 16);   // 留 60° 缺口
        g.restore();
        arrow(9.4, 5.7, -0.698, mark);          // 缺口处沿弧切向的箭头
        return;
    }
    if (id == "views") {                        // 制图模板：三视图（主/俯/侧）
        g.drawRect(QRectF(4.5, 4.5, 9.0, 7.0));     // 主视图
        g.drawRect(QRectF(4.5, 13.5, 9.0, 6.0));    // 俯视图
        g.save();
        g.setPen(QPen(mark, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawRect(QRectF(15.5, 4.5, 4.5, 7.0));    // 侧视图（强调）
        g.restore();
        return;
    }
    if (id == "cancel") {                       // 取消当前绘制：圆圈斜杠
        g.drawEllipse(QPointF(12, 12), 7.5, 7.5);
        g.save();
        g.setPen(QPen(mark, 2.0, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(7.0, 17.0), QPointF(17.0, 7.0));
        g.restore();
        return;
    }
    if (id == "plugin") {                       // 插件：拼图块
        QPainterPath piece;
        piece.moveTo(4.5, 7.5);
        piece.lineTo(9.5, 7.5);
        piece.cubicTo(9.5, 3.5, 14.5, 3.5, 14.5, 7.5);   // 上方凸榫
        piece.lineTo(19.5, 7.5);
        piece.lineTo(19.5, 16.5);
        piece.lineTo(4.5, 16.5);
        piece.closeSubpath();
        g.drawPath(piece);
        dot(12.0, 13.0, mark, 1.6);
        return;
    }
    if (id == "options") {                      // 选项：三档滑杆
        const double ys[3] = {7.0, 12.0, 17.0};
        const double kx[3] = {9.0, 15.0, 11.5};
        for (int i = 0; i < 3; ++i) {
            g.save();
            g.setPen(QPen(guide, 1.6, Qt::SolidLine, Qt::RoundCap));
            g.drawLine(QPointF(4.5, ys[i]), QPointF(19.5, ys[i]));
            g.restore();
            dot(kx[i], ys[i], mark, 2.3);
        }
        return;
    }
    if (id == "log") {                          // 日志：文档 + 条目行
        g.drawRoundedRect(QRectF(5.0, 3.5, 14.0, 17.0), 2.0, 2.0);
        g.save();
        g.setPen(QPen(guide, 1.5, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(8.0, 8.5),  QPointF(16.0, 8.5));
        g.drawLine(QPointF(8.0, 12.0), QPointF(16.0, 12.0));
        g.drawLine(QPointF(8.0, 15.5), QPointF(13.0, 15.5));
        g.restore();
        return;
    }
    if (id == "about") {                        // 关于：信息圆标
        g.drawEllipse(QPointF(12, 12), 8.0, 8.0);
        text(QStringLiteral("i"), mark, 14, QRectF(5.0, 4.0, 14.0, 16.0));
        return;
    }
    if (id == "exit") {                         // 退出：箭头穿门而出
        g.drawRect(QRectF(4.5, 4.0, 9.0, 16.0));
        g.save();
        g.setPen(QPen(mark, 1.8, Qt::SolidLine, Qt::RoundCap));
        g.drawLine(QPointF(13.5, 12.0), QPointF(16.5, 12.0));
        g.restore();
        arrow(20.5, 12.0, 0.0, mark);
        return;
    }

    // 兜底：画个空心方块，避免"看不见的按钮"
    g.drawRect(QRectF(5.5, 5.5, 13, 13));
}

} // namespace detail

// 生成工具/菜单图标。id 见 paintGlyph 内的分支。
inline QIcon make(const QString& id)
{
    const qreal dpr = 2.0;   // 固定 2x 光栅化，缩放到 16/20/24px 都保持锐利
    QPixmap pm(int(detail::kSize * dpr), int(detail::kSize * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setRenderHint(QPainter::TextAntialiasing, true);
    detail::paintGlyph(g, id);
    g.end();
    return QIcon(pm);
}

// 纯色色块图标：工具条颜色按钮与预设色菜单项共用
inline QIcon swatch(const QColor& c)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(detail::kSize * dpr), int(detail::kSize * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setPen(QPen(QColor(0x55, 0x5D, 0x6B), 1.2));
    g.setBrush(c);
    g.drawRoundedRect(QRectF(3.5, 5.5, 17, 13), 2.5, 2.5);
    g.end();
    return QIcon(pm);
}

// 线宽示意图标：按 px 画一条对应粗细的横线，下拉框里一眼可辨
inline QIcon widthBar(double px)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(detail::kSize * dpr), int(detail::kSize * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setPen(QPen(inkColor(), px, Qt::SolidLine, Qt::RoundCap));
    g.drawLine(QPointF(3.5, 12.0), QPointF(20.5, 12.0));
    g.end();
    return QIcon(pm);
}

// 线型示意图标：用对应 Qt::PenStyle 画一条横线，下拉框里一眼可辨。
// 粗线档（GB/T 14665-2012 表1）用更粗的笔，使「虚线 / 粗虚线」「点画线 / 粗点画线」可区分。
inline QIcon lineStyleIcon(LineStyle ls)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(detail::kSize * dpr), int(detail::kSize * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    g.setRenderHint(QPainter::Antialiasing, true);
    const qreal w = lineStyleIsThick(ls) ? 2.6 : 1.4;
    QPen pen(inkColor(), w, toPenStyle(ls), Qt::RoundCap, Qt::RoundJoin);
    g.setPen(pen);
    g.drawLine(QPointF(3.5, 12.0), QPointF(20.5, 12.0));
    g.end();
    return QIcon(pm);
}

} // namespace icons
} // namespace SketchCAD
