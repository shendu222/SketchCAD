#pragma once
// DbConfigDialog — 数据库配置对话框
//
// 编辑连接信息（SQLite 文件型 / MySQL 服务器型）并保存到本地配置文件。
// 对话框本身不直接持久化：点「确定」时通过 configAccepted 信号把填写内容交回
// MainWindow，由 MainWindow 负责 DrawingDb::setConnectionInfo + saveConfigToFile
// + 重连，并刷新状态栏/菜单可用态（保持单一责任，避免对话框依赖主窗口内部状态）。

#include <QDialog>
#include <QString>

class QComboBox;
class QGroupBox;
class QLineEdit;

namespace SketchCAD {

class DbConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit DbConfigDialog(QWidget* parent = nullptr);

    // 用当前 DrawingDb 连接信息预填表单（打开对话框或语言重建时调用）
    void refresh();

signals:
    // driver ∈ {"QSQLITE","QMYSQL"}；其余为连接参数字段
    void configAccepted(const QString& driver, const QString& dbPath,
                        const QString& host, const QString& dbName,
                        const QString& user, const QString& pass, int port);

private slots:
    void onDriverChanged(int index);
    void onBrowse();
    void onTest();
    void onAccepted();

private:
    QComboBox* driverCombo_ = nullptr;
    QGroupBox* sqliteGroup_ = nullptr;
    QLineEdit* pathEdit_    = nullptr;
    QGroupBox* mysqlGroup_  = nullptr;
    QLineEdit* hostEdit_    = nullptr;
    QLineEdit* portEdit_    = nullptr;
    QLineEdit* mysqlDbEdit_ = nullptr;
    QLineEdit* userEdit_    = nullptr;
    QLineEdit* passEdit_    = nullptr;
};

} // namespace SketchCAD
