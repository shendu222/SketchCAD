#include "ThemeManager.h"

#include <QApplication>
#include <QStyleHints>

namespace SketchCAD {

ThemeManager* ThemeManager::instance() {
    static ThemeManager m;          // 进程单例（C++11 静态局部，线程安全）
    return &m;
}

void ThemeManager::setMode(Mode m) {
    mode_ = m;
    applyToApp();
    emit themeChanged(effectiveMode());
}

void ThemeManager::applyToApp() {
    // 主题仅影响视觉；无 QApplication（如无头自测）时静默跳过，不崩溃。
    if (!qApp) return;
    QStyleHints* sh = qApp->styleHints();
    if (!sh) return;
    // Qt6：QStyleHints::setColorScheme 驱动原生样式（含 Windows 11 样式）的亮暗翻转；
    // Unknown 表示交还系统（跟随系统）。非白名单值在 AppSettings 校验层已拦截，
    // 这里只处理受信的三态。
    switch (mode_) {
    case Mode::Light:  sh->setColorScheme(Qt::ColorScheme::Light);  break;
    case Mode::Dark:  sh->setColorScheme(Qt::ColorScheme::Dark);   break;
    case Mode::System:sh->setColorScheme(Qt::ColorScheme::Unknown); break;
    }
}

bool ThemeManager::isDark() const {
    if (mode_ == Mode::Light) return false;
    if (mode_ == Mode::Dark)  return true;
    // System：看系统实际解析出的配色方案；无 qApp 或解析为 Unknown 时回落亮色。
    if (!qApp) return false;
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QColor ThemeManager::canvasColor() const {
    // 暗色画布用近黑灰，亮色沿用既有的浅灰蓝（(245,247,250)）。
    return isDark() ? QColor(38, 40, 45) : QColor(245, 247, 250);
}

ThemeManager::Mode ThemeManager::effectiveMode() const {
    if (mode_ != Mode::System) return mode_;
    return isDark() ? Mode::Dark : Mode::Light;
}

QString ThemeManager::modeToString(Mode m) {
    switch (m) {
    case Mode::System: return QStringLiteral("system");
    case Mode::Dark:   return QStringLiteral("dark");
    case Mode::Light:  return QStringLiteral("light");
    }
    return QStringLiteral("light");
}

ThemeManager::Mode ThemeManager::modeFromString(const QString& s) {
    if (s == QLatin1String("system")) return Mode::System;
    if (s == QLatin1String("dark"))   return Mode::Dark;
    return Mode::Light;   // light / 空 / 未知 → Light（默认）
}

} // namespace SketchCAD
