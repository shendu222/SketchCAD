#pragma once
#include <QObject>
#include <QColor>

namespace SketchCAD {

// 内置界面主题：system=跟随系统 / light=亮色 / dark=暗色。
//
// 设计为进程单例，职责单一：
//   1) 据 Mode 应用主题到 QApplication（Qt6 机制：QStyleHints::setColorScheme——
//      原生 Windows 11 样式下即可驱动菜单/工具栏/对话框等原生控件亮暗翻转，
//      无需改用 Fusion 破坏项目既有的原生外观选择）；
//   2) 暴露画布背景色 canvasColor()，随主题翻色（绘制区是 CAD 最显眼的"控件"，
//      必须显式跟随，否则会留在硬编码的亮灰蓝）；
//   3) 主题变更时广播 themeChanged，订阅方（GraphicsView）自刷新。
//
// 接线：
//   * main() 在 QApplication 构造后据注册表 display/theme（默认 light）调用 setMode 一次；
//   * OptionsDialog 改键 → AppSettings::settingChanged(kTheme) →
//     MainWindow::applySettingLive → ThemeManager::setMode 即时重绘（含运行时切换）。
class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };
    Q_ENUM(Mode)

    static ThemeManager* instance();                  // 进程单例（惰性创建）
    Mode mode() const { return mode_; }
    void setMode(Mode m);                             // 改视觉 + 广播 themeChanged（持久化由 AppSettings 负责）
    QColor canvasColor() const;                      // 当前画布背景（随主题翻色）
    Mode effectiveMode() const;                      // System 已解析为实际 Light/Dark
    static QString modeToString(Mode m);
    static Mode modeFromString(const QString& s);    // 未知/空 → Light（默认）

signals:
    // 实际生效模式（System 模式下已解析为系统当前的 Light/Dark）。
    void themeChanged(SketchCAD::ThemeManager::Mode effective);

private:
    explicit ThemeManager(QObject* parent = nullptr) : QObject(parent), mode_(Mode::Light) {}
    void applyToApp();
    bool isDark() const;

    Mode mode_ = Mode::Light;
};

} // namespace SketchCAD
