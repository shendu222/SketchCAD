#include "ModifyInputDialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>

namespace SketchCAD {

ModifyInputDialog::ModifyInputDialog(Mode m, const QString& title,
                                     int selectedId1, int selectedId2,
                                     QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    auto* form = new QFormLayout(this);

    auto addRow = [&](const QString& lbl, QWidget* w) {
        form->addRow(new QLabel(lbl), w);
    };
    auto addD = [&](const QString& lbl, double& v, double lo, double hi, int prec = 2) {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(lo, hi);
        sb->setDecimals(prec);
        sb->setValue(v);
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         this, [&v](double x){ v = x; });
        addRow(lbl, sb);
    };
    auto addI = [&](const QString& lbl, int& v, int lo, int hi) {
        auto* sb = new QSpinBox(this);
        sb->setRange(lo, hi);
        sb->setValue(v);
        QObject::connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, [&v](int x){ v = x; });
        addRow(lbl, sb);
    };

    switch (m) {
    case M_Move:
        addD(QStringLiteral("Δx (mm)"), res_.dx, -1e6, 1e6);
        addD(QStringLiteral("Δy (mm)"), res_.dy, -1e6, 1e6);
        break;
    case M_Rotate:
        addD(QStringLiteral("旋转中心 X"), res_.cx, -1e6, 1e6);
        addD(QStringLiteral("旋转中心 Y"), res_.cy, -1e6, 1e6);
        addD(QStringLiteral("角度（度，CCW 为正）"), res_.angleDeg, -360.0, 360.0);
        break;
    case M_Scale:
        addD(QStringLiteral("缩放中心 X"), res_.cx, -1e6, 1e6);
        addD(QStringLiteral("缩放中心 Y"), res_.cy, -1e6, 1e6);
        addD(QStringLiteral("缩放因子（>0）"), res_.factor, 1e-6, 1e6, 4);
        break;
    case M_Lengthen:
        addI(QStringLiteral("端点（0=起点 / 1=终点）"), res_.side, 0, 1);
        addD(QStringLiteral("Δ长度 (mm，正=延长 负=缩短)"), res_.deltaMm, -1e6, 1e6);
        break;
    case M_Offset:
        addD(QStringLiteral("偏移距离 (mm，左正右负)"), res_.distMm, -1e6, 1e6);
        break;
    case M_Break:
        addD(QStringLiteral("打断点 X"), res_.px, -1e6, 1e6);
        addD(QStringLiteral("打断点 Y"), res_.py, -1e6, 1e6);
        break;
    case M_Divide: {
        // 段数（默认 2）
        addI(QStringLiteral("等分段数 n（≥2）"), res_.n, 2, 999);
        if (selectedId1 >= 0) res_.n = res_.n;
        break;
    }
    case M_Fillet:
        // id1 / id2 默认值来自 selectedId1/2
        res_.id1 = selectedId1;
        res_.id2 = selectedId2;
        {
            auto* s1 = new QSpinBox(this); s1->setRange(-1, 99999); s1->setValue(res_.id1);
            QObject::connect(s1, QOverload<int>::of(&QSpinBox::valueChanged),
                             this, [&](int v){ res_.id1 = v; });
            addRow(QStringLiteral("线段 1 ID"), s1);
            auto* s2 = new QSpinBox(this); s2->setRange(-1, 99999); s2->setValue(res_.id2);
            QObject::connect(s2, QOverload<int>::of(&QSpinBox::valueChanged),
                             this, [&](int v){ res_.id2 = v; });
            addRow(QStringLiteral("线段 2 ID"), s2);
        }
        addD(QStringLiteral("圆角半径 (mm，=0 即倒角)"), res_.radiusMm, 0.0, 1e6);
        break;
    case M_Stretch:
        addD(QStringLiteral("基点 X (端点坐标)"), res_.baseX, -1e6, 1e6);
        addD(QStringLiteral("基点 Y"), res_.baseY, -1e6, 1e6);
        addD(QStringLiteral("目标 X"), res_.toX, -1e6, 1e6);
        addD(QStringLiteral("目标 Y"), res_.toY, -1e6, 1e6);
        break;
    case M_MirrorLine:
        addD(QStringLiteral("轴点 1 X"), res_.p1.x, -1e6, 1e6);
        addD(QStringLiteral("轴点 1 Y"), res_.p1.y, -1e6, 1e6);
        addD(QStringLiteral("轴点 2 X"), res_.p2.x, -1e6, 1e6);
        addD(QStringLiteral("轴点 2 Y"), res_.p2.y, -1e6, 1e6);
        break;
    case M_Align: {
        auto* box = new QComboBox(this);
        box->addItems({"left", "centerx", "right", "topy", "centery", "bottom"});
        box->setCurrentText(res_.kind);
        QObject::connect(box, &QComboBox::currentTextChanged, this,
                         [&](const QString& s){ res_.kind = s; });
        addRow(QStringLiteral("对齐方式"), box);
        break;
    }
    }

    auto* btn = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btn, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btn, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(btn);
}

} // namespace SketchCAD