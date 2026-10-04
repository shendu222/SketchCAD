#include "StatusBar.h"
#include "core/CommandPrompt.h"
#include <QLabel>
#include <QToolButton>

namespace SketchCAD {

StatusBar::StatusBar(QWidget* p) : QStatusBar(p) {
    lCmd   = new QLabel(QStringLiteral("就绪"));   lCmd->setMinimumWidth(120);
    lOpt   = new QLabel(QString());                lOpt->setMinimumWidth(60);
    lCoord = new QLabel(QStringLiteral("X:0.00  Y:0.00  Z:0.00")); lCoord->setMinimumWidth(170);
    lPlane = new QLabel(QStringLiteral("平面:XY")); lPlane->setMinimumWidth(70);
    lCount = new QLabel(QStringLiteral("图元:0"));  lCount->setMinimumWidth(60);
    lScale = new QLabel(QStringLiteral("比例:4.00")); lScale->setMinimumWidth(70);
    lStats = new QLabel(QString());               lStats->setMinimumWidth(150);   // P0-04 数据面板
    addWidget(lCmd); addWidget(lOpt); addWidget(lCoord); addWidget(lPlane);
    addWidget(lCount); addWidget(lScale); addWidget(lStats);

    // ---- 上次自动保存时间（点击手动触发一次自动保存）----
    // 用 QToolButton 而非 QLabel：QLabel 无 clicked 信号，需另做事件过滤器；
    // 扁平 + autoRaise 使其外观与状态栏文本一致。
    btnSave_ = new QToolButton(this);
    btnSave_->setText(QStringLiteral("自动保存:—"));
    btnSave_->setToolTip(QStringLiteral("点击立即执行一次自动保存"));
    btnSave_->setAutoRaise(true);
    btnSave_->setFocusPolicy(Qt::NoFocus);      // 不抢命令行焦点
    btnSave_->setMinimumWidth(120);
    addPermanentWidget(btnSave_);
    connect(btnSave_, &QToolButton::clicked, this, &StatusBar::manualSaveRequested);

    // ---- 可点击开关组：正交 / 极轴 / 捕捉 / 追踪 / 线宽 ----
    // 按钮只发 toggleRequested；模型回写经 setToggleState（信号阻塞防回环）。
    struct ToggleDef { Toggle key; const char* text; const char* tip; };
    const ToggleDef defs[5] = {
        { Toggle::Ortho, "正交", "正交模式 (F8)：方向锁定到水平/垂直" },
        { Toggle::Polar, "极轴", "极轴追踪 (F10)：按增量角度追踪落点" },
        { Toggle::Snap,  "捕捉", "栅格吸附 (F9)：坐标量化到栅格步长" },
        { Toggle::Track, "追踪", "对象捕捉追踪 (F11)：沿捕捉点对齐方向追踪" },
        { Toggle::Lwt,   "线宽", "线宽显示：按图元实际线宽渲染（关=一律细线）" },
    };
    for (const auto& d : defs) {
        auto* b = new QToolButton(this);
        b->setText(QString::fromUtf8(d.text));
        b->setToolTip(QString::fromUtf8(d.tip));
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::NoFocus);          // 不抢命令行焦点
        const Toggle key = d.key;
        connect(b, &QToolButton::toggled, this, [this, key](bool on) {
            emit toggleRequested(key, on);
        });
        btns_[(int)key] = b;
        addPermanentWidget(b);
    }

    setStyleSheet("QStatusBar::item{border:0;}");
}

void StatusBar::setCommand(const QString& s) { lCmd->setText(s); }

void StatusBar::setOptions(const QStringList& opts) {
    const QString t = CommandPrompt::formatOptionPrompt(opts);
    lOpt->setText(t);
    lOpt->setVisible(!t.isEmpty());
}

void StatusBar::setToggleState(Toggle key, bool on) {
    if (key < Toggle::Ortho || key > Toggle::Lwt) return;
    QToolButton* b = btns_[(int)key];
    if (!b) return;
    const QSignalBlocker blocker(b);   // 防回环：回写不触发 toggled → toggleRequested
    b->setChecked(on);
}

void StatusBar::setCoord(double x, double y, double z) {
    // 按当前单位口径格式化（存储为 mm，显示换算 + 精度 + 短后缀）
    lCoord->setText(QString("X:%1  Y:%2  Z:%3")
                        .arg(formatLengthMm(x, units_), formatLengthMm(y, units_))
                        .arg(formatLengthMm(z, units_)));
}
void StatusBar::setPlaneInfo(const QString& s) { lPlane->setText("平面:" + s); }
void StatusBar::setCount(int n) { lCount->setText("图元:" + QString::number(n)); }

// P0-04 数据面板：「图元 N / 场景项 M / 打开 Xms」。openMs<=0 时隐藏打开耗时段。
void StatusBar::setLoadStats(int elements, int items, double openMs) {
    QString s = QStringLiteral("场景项:%1").arg(items);
    if (openMs > 0) s += QStringLiteral(" / 打开:%1ms").arg(openMs, 0, 'f', 0);
    lStats->setText(s);
    Q_UNUSED(elements);   // 图元数已有 lCount 呈现；保留参数以备面板合并展示
}
void StatusBar::setScale(double s) { lScale->setText("比例:" + QString::number(s, 'f', 2)); }
void StatusBar::setDrawScale(const QString& label) { lScale->setText("比例:" + label); }

void StatusBar::setLastAutosave(const QDateTime& t) {
    if (!autosaveOn_) { btnSave_->setText(QStringLiteral("自动保存:关")); return; }
    btnSave_->setText(t.isValid()
        ? QStringLiteral("上次自动保存 %1").arg(t.toString(QStringLiteral("HH:mm")))
        : QStringLiteral("自动保存:—"));
}

void StatusBar::setAutosaveEnabled(bool on) {
    autosaveOn_ = on;
    if (!on) btnSave_->setText(QStringLiteral("自动保存:关"));
    else     btnSave_->setText(QStringLiteral("自动保存:—"));
}

void StatusBar::setUnitsMeta(const UnitsMeta& u) {
    units_ = u;
    lCoord->setMinimumWidth(u.length == LengthUnit::Mm ? 170 : 210);   // 带后缀更宽
    // 立即以新口径重刷一次（下个鼠标事件前不至于残留旧格式）
    // 坐标值不在本类持有，仅下次 cursorMoved 时生效；单位面板确定后
    // MainWindow 会同步触发一次 fitView/刷新，视觉无跳变。
}

} // namespace SketchCAD
