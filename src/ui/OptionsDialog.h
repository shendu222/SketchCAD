#pragma once
#include <QDialog>
#include <QHash>
#include <QVariant>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;

namespace SketchCAD {

class AppSettings;

// 统一「选项」对话框：左侧分类树 + 右侧表单。
//
// 交互契约：
//   * [应用]/[确定] 把表单写入 AppSettings（逐键 setValue → settingChanged 信号
//     → 主窗口即时生效，即"预览"）；
//   * [取消] 用打开时的快照逐键写回（走同一信号通道）——本次会话内的即时预览
//     改动全部回滚，包括已点过「应用」的；
//   * [重置为默认值] 仅把表单控件置回默认值（草稿态），须再点 [确定]/[应用]
//     才真正生效；点击时二次确认。
//   * 应用/回滚后检查底层写入状态，失败弹窗提示（禁止静默失败）。
class OptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OptionsDialog(AppSettings* settings, QWidget* parent = nullptr);

private slots:
    void onApply();
    void onCancel();
    void onResetDefaults();

private:
    QWidget* buildGeneralPage();
    QWidget* buildDisplayPage();
    QWidget* buildFilesPage();
    QWidget* buildAutosavePage();
    QWidget* buildPrintPage();
    QWidget* buildUserPage();
    void addPage(const QString& title, QWidget* page);

    void loadWidgets();                 // 控件 <- AppSettings 当前值
    void loadWidgetsFromDefaults();     // 控件 <- 默认值（重置草稿，不落盘）
    void collectWidgets();              // 控件 -> AppSettings（逐键 setValue）
    bool applyAndCheck();               // collect + sync + 失败弹窗；返回是否无错
    void warnWriteFailed();
    static QWidget* formWrap(QWidget* inner);   // 页面统一外边距

    AppSettings* s_ = nullptr;
    QTreeWidget* catTree_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QPushButton* btnOk_ = nullptr;
    QPushButton* btnCancel_ = nullptr;
    QPushButton* btnApply_ = nullptr;
    QPushButton* btnReset_ = nullptr;

    // 打开时的全键快照：取消回滚的基线（覆盖"应用过再取消"的会话预览回滚）
    QHash<QString, QVariant> snapshot_;

    // 常规
    QComboBox* langBox_ = nullptr;
    QSpinBox* recentSpin_ = nullptr;
    // 显示
    QCheckBox* chkGrid_ = nullptr;
    QCheckBox* chkSnap_ = nullptr;
    QDoubleSpinBox* stepSpin_ = nullptr;
    QCheckBox* chkOrtho_ = nullptr;
    QCheckBox* chkDyn_ = nullptr;
    QCheckBox* chkLwd_ = nullptr;
    QCheckBox* chkGpu_ = nullptr;
    QCheckBox* chkOff_ = nullptr;
    // 文件位置
    QLineEdit* saveDirEdit_ = nullptr;
    QLineEdit* tmplEdit_ = nullptr;
    // 自动保存
    QComboBox* intervalBox_ = nullptr;
    QSpinBox* backupSpin_ = nullptr;
    QSpinBox* diskSpin_ = nullptr;
    // 打印
    QComboBox* printerBox_ = nullptr;
    // 用户
    QLineEdit* userNameEdit_ = nullptr;
    QLineEdit* companyEdit_ = nullptr;
};

} // namespace SketchCAD
