#pragma once
#include <QDialog>
#include <QVariantMap>

class QLabel;
class QListWidget;

namespace SketchCAD {

// 诊断包导出对话框：一键打包最近日志 + 崩溃转储 + 系统/图纸元信息
// （不含几何内容）到用户目录 ZIP。
//
// 两个入口共用：
//   1. 启动提示「上次异常退出，是否发送诊断包/查看日志」（showAbnormalExitNotice）；
//   2. 帮助菜单「导出诊断包...」。
// 打包逻辑全部在 core/DiagBundle（纯 core 可无头直测），本对话框只做薄壳。
class DiagnosticsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiagnosticsDialog(const QVariantMap& drawingMeta, QWidget* parent = nullptr);

private:
    void doExport();
    void openLogDir();

    QVariantMap   meta_;      // 图纸元信息（计数类字段，由 MainWindow 组装）
    QListWidget*  fileList_ = nullptr;
};
} // namespace SketchCAD
