#include "DbConfigDialog.h"
#include "core/DrawingDb.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QStandardItemModel>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDir>

namespace SketchCAD {

DbConfigDialog::DbConfigDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("数据库配置"));
    setMinimumWidth(440);

    auto* mainLayout = new QVBoxLayout(this);

    // ---- 数据库类型 ----
    driverCombo_ = new QComboBox(this);
    driverCombo_->addItem(tr("SQLite（文件型，零配置）"), QStringLiteral("QSQLITE"));
    driverCombo_->addItem(tr("MySQL（服务器型）"), QStringLiteral("QMYSQL"));
    // 当前 Qt 构建可能缺失 QMYSQL 插件：禁用该选项并给出提示，避免误选后拿到硬编码驱动错误。
    const bool mysqlOk = QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"));
    if (!mysqlOk) {
        const int idx = driverCombo_->findData(QStringLiteral("QMYSQL"));
        if (idx >= 0) {
            if (auto* m = qobject_cast<QStandardItemModel*>(driverCombo_->model())) {
                if (QStandardItem* item = m->item(idx)) item->setEnabled(false);
            }
        }
    }
    connect(driverCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DbConfigDialog::onDriverChanged);
    mainLayout->addWidget(new QLabel(tr("数据库类型：")));
    mainLayout->addWidget(driverCombo_);
    if (!mysqlOk) {
        auto* note = new QLabel(tr(
            "⚠ MySQL 驱动不可用：当前环境缺少 Qt 的 QMYSQL 插件（qsqlmysql.dll 与 libmysql.dll）。\n"
            "已禁用 MySQL 连接，数据库功能默认使用内置 SQLite，可正常工作。\n"
            "如需启用 MySQL，请将 qsqlmysql.dll 放入程序目录的 sqldrivers/ 子目录并配置 libmysql.dll 路径后重启。"), this);
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color: #c8761a;"));
        mainLayout->addWidget(note);
    }

    // ---- SQLite 设置 ----
    sqliteGroup_ = new QGroupBox(tr("SQLite 设置"), this);
    auto* sf = new QFormLayout(sqliteGroup_);
    pathEdit_ = new QLineEdit(sqliteGroup_);
    pathEdit_->setPlaceholderText(tr("留空则使用默认：~/SketchCAD/sketchcad_drawings.db"));
    auto* browseBtn = new QPushButton(tr("浏览..."), sqliteGroup_);
    connect(browseBtn, &QPushButton::clicked, this, &DbConfigDialog::onBrowse);
    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(pathEdit_);
    pathRow->addWidget(browseBtn);
    sf->addRow(tr("数据库文件："), pathRow);
    mainLayout->addWidget(sqliteGroup_);

    // ---- MySQL 设置 ----
    mysqlGroup_ = new QGroupBox(tr("MySQL 设置"), this);
    auto* mf = new QFormLayout(mysqlGroup_);
    hostEdit_ = new QLineEdit(mysqlGroup_);
    hostEdit_->setPlaceholderText(tr("数据库服务器地址，留空则 localhost"));
    portEdit_ = new QLineEdit(mysqlGroup_);
    portEdit_->setFixedWidth(90);
    portEdit_->setText(QStringLiteral("3306"));
    mysqlDbEdit_ = new QLineEdit(mysqlGroup_);
    mysqlDbEdit_->setPlaceholderText(tr("数据库名，如 sketchcad"));
    userEdit_ = new QLineEdit(mysqlGroup_);
    userEdit_->setPlaceholderText(tr("登录用户名，如 root"));
    passEdit_ = new QLineEdit(mysqlGroup_);
    passEdit_->setEchoMode(QLineEdit::Password);
    mf->addRow(tr("主机："), hostEdit_);
    mf->addRow(tr("端口："), portEdit_);
    mf->addRow(tr("数据库名："), mysqlDbEdit_);
    mf->addRow(tr("用户名："), userEdit_);
    mf->addRow(tr("密码："), passEdit_);
    mainLayout->addWidget(mysqlGroup_);

    // ---- 测试连接 ----
    auto* testBtn = new QPushButton(tr("测试连接"), this);
    connect(testBtn, &QPushButton::clicked, this, &DbConfigDialog::onTest);
    mainLayout->addWidget(testBtn);

    // ---- 确定 / 取消 ----
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &DbConfigDialog::onAccepted);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);

    onDriverChanged(driverCombo_->currentIndex());
}

void DbConfigDialog::refresh() {
    DrawingDb* db = DrawingDb::instance();
    const QString drv = db->driver();
    int idx = driverCombo_->findData(drv);
    if (idx < 0) idx = 0;
    // 已保存配置若是 QMYSQL 但当前驱动缺失，则回落 SQLite，避免选中被禁用的项。
    if (drv == QStringLiteral("QMYSQL") && !QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL")))
        idx = 0;
    driverCombo_->setCurrentIndex(idx);
    pathEdit_->setText(db->dbPath());
    hostEdit_->setText(db->host());
    portEdit_->setText(QString::number(db->port()));
    mysqlDbEdit_->setText(db->dbName());
    userEdit_->setText(db->user());
    passEdit_->setText(db->pass());
    onDriverChanged(idx);
}

void DbConfigDialog::onDriverChanged(int index) {
    const bool isSqlite = driverCombo_->itemData(index).toString() == QStringLiteral("QSQLITE");
    sqliteGroup_->setVisible(isSqlite);
    mysqlGroup_->setVisible(!isSqlite);
}

void DbConfigDialog::onBrowse() {
    const QString cur = pathEdit_->text().isEmpty() ? QDir::homePath() : pathEdit_->text();
    const QString path = QFileDialog::getSaveFileName(this, tr("选择 SQLite 数据库文件"),
        cur, tr("SQLite 数据库 (*.db *.sqlite);;所有文件 (*.*)"));
    if (!path.isEmpty()) pathEdit_->setText(path);
}

void DbConfigDialog::onTest() {
    const QString driver = driverCombo_->currentData().toString();
    // 防御：驱动缺失时直接提示，不再走会报硬编码错误的测试路径。
    if (!QSqlDatabase::isDriverAvailable(driver)) {
        QMessageBox::warning(this, tr("测试连接"),
            tr("当前 Qt 构建未包含该数据库驱动（%1）。\n请改用内置的 SQLite，或将对应驱动插件放入程序目录的 sqldrivers/ 子目录后重启。").arg(driver));
        return;
    }
    const QString dbPath = pathEdit_->text().trimmed();
    const QString host   = hostEdit_->text().trimmed();
    const int     port   = portEdit_->text().toInt();
    const QString dbName = mysqlDbEdit_->text().trimmed();
    const QString user   = userEdit_->text();
    const QString pass   = passEdit_->text();

    const QString err = DrawingDb::testConnection(driver, dbPath, host, port, dbName, user, pass);
    if (err.isEmpty())
        QMessageBox::information(this, tr("测试连接"), tr("连接成功。"));
    else
        QMessageBox::warning(this, tr("测试连接"), tr("连接失败：\n%1").arg(err));
}

void DbConfigDialog::onAccepted() {
    emit configAccepted(driverCombo_->currentData().toString(),
                        pathEdit_->text().trimmed(),
                        hostEdit_->text().trimmed(),
                        mysqlDbEdit_->text().trimmed(),
                        userEdit_->text(),
                        passEdit_->text(),
                        portEdit_->text().toInt());
    accept();
}

} // namespace SketchCAD
