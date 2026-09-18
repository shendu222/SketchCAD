#pragma once
#include <QDialog>
#include <QString>
#include "core/Vec.h"

namespace SketchCAD {

// 修改菜单参数输入对话框。
// 根据 mode 切换字段：
//   Move        : dx, dy
//   Rotate      : cx, cy, angleDeg
//   Scale       : cx, cy, factor
//   Lengthen    : side(0/1), deltaMm
//   Offset      : distMm
//   Break       : px, py
//   Divide      : id, n
//   Fillet      : id1, id2, radiusMm
//   Mirror      : axis(h/v/line), [p1, p2]
//   Stretch     : baseX, baseY, toX, toY
//   Align       : kind(left/centerx/right/topy/centery/bottom)
// 通过 selected 字段预填两个 ID（fillet 用）。
class ModifyInputDialog : public QDialog {
public:
    enum Mode { M_Move, M_Rotate, M_Scale, M_Lengthen, M_Offset,
                M_Break, M_Divide, M_Fillet, M_Stretch, M_MirrorLine, M_Align };

    // 用户接受 OK 后回填的字段。
    struct Result {
        // Move
        double dx = 10.0, dy = 0.0;
        // Rotate / Scale
        double cx = 0.0, cy = 0.0;
        double angleDeg = 90.0;
        double factor   = 2.0;
        // Lengthen
        int    side = 1;
        double deltaMm = 10.0;
        // Offset
        double distMm = 5.0;
        // Break
        double px = 0.0, py = 0.0;
        // Divide
        int    n = 2;
        // Fillet
        int    id1 = -1, id2 = -1;
        double radiusMm = 5.0;
        // Mirror (line axis)
        Vec3   p1, p2{1, 0, 0};
        // Stretch
        double baseX = 0.0, baseY = 0.0, toX = 0.0, toY = 0.0;
        // Align
        QString kind = QStringLiteral("left");
    };

    ModifyInputDialog(Mode m, const QString& title, int selectedId1 = -1, int selectedId2 = -1,
                      QWidget* parent = nullptr);

    Result result() const { return res_; }

private:
    Result res_;
};

} // namespace SketchCAD