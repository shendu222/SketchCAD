#include "core/I18n.h"
#include "OptionsDialog.h"

#include "core/AppSettings.h"
#include "core/DrawingDb.h"

#include <QTreeWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QStandardItemModel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QPrinterInfo>
#include <QDir>

namespace SketchCAD {

using namespace SettingKeys;

namespace {
// 组合框 data 存实际值、text 存显示文案，避免"第 N 项"这种魔法下标散落各处
void addComboItem(QComboBox* box, const QString& text, const QVariant& data) {
    box->addItem(text, data);
}
int comboIndexOfData(const QComboBox* box, const QVariant& data) {
    for (int i = 0; i < box->count(); ++i)
        if (box->itemData(i) == data) return i;
    return 0;
}
} // namespace

OptionsDialog::OptionsDialog(AppSettings* settings, QWidget* parent)
    : QDialog(parent), s_(settings) {
    Q_ASSERT(s_);
    setWindowTitle(SketchCAD::I18n::t(tr("选项")));
    setModal(true);
    resize(720, 540);

    auto* rootLay = new QVBoxLayout(this);

    // 左分类树 + 右页面栈
    auto* body = new QHBoxLayout();
    catTree_ = new QTreeWidget(this);
    catTree_->setFixedWidth(150);
    catTree_->setHeaderLabel(SketchCAD::I18n::t(tr("分类")));
    catTree_->setRootIsDecorated(false);
    pages_ = new QStackedWidget(this);
    body->addWidget(catTree_);
    body->addWidget(pages_, 1);
    rootLay->addLayout(body, 1);

    // 底部按钮：重置（左）| stretch | 确定 取消 应用（右）
    btnReset_   = new QPushButton(SketchCAD::I18n::t(tr("重置为默认值")), this);
    btnOk_      = new QPushButton(SketchCAD::I18n::t(tr("确定")), this);
    btnCancel_  = new QPushButton(SketchCAD::I18n::t(tr("取消")), this);
    btnApply_   = new QPushButton(SketchCAD::I18n::t(tr("应用")), this);
    btnOk_->setDefault(true);
    auto* btnLay = new QHBoxLayout();
    btnLay->addWidget(btnReset_);
    btnLay->addStretch(1);
    btnLay->addWidget(btnOk_);
    btnLay->addWidget(btnCancel_);
    btnLay->addWidget(btnApply_);
    rootLay->addLayout(btnLay);

    // 七个分类页（顺序 = 需求规定）
    addPage(SketchCAD::I18n::t(tr("常规")),     buildGeneralPage());
    addPage(SketchCAD::I18n::t(tr("显示")),     buildDisplayPage());
    addPage(SketchCAD::I18n::t(tr("文件位置")), buildFilesPage());
    addPage(SketchCAD::I18n::t(tr("自动保存")), buildAutosavePage());
    addPage(SketchCAD::I18n::t(tr("打印")),     buildPrintPage());
    addPage(SketchCAD::I18n::t(tr("用户")),     buildUserPage());
    addPage(SketchCAD::I18n::t(tr("隐私")),     buildPrivacyPage());
    addPage(SketchCAD::I18n::t(tr("数据库")),   buildDatabasePage());
    addPage(SketchCAD::I18n::t(tr("AI 助手")),  buildAiPage());

    connect(catTree_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* cur, QTreeWidgetItem*) {
        if (cur) pages_->setCurrentIndex(catTree_->indexOfTopLevelItem(cur));
    });
    catTree_->setCurrentItem(catTree_->topLevelItem(0));

    connect(btnOk_,     &QPushButton::clicked, this, [this] { if (applyAndCheck()) accept(); });
    connect(btnCancel_, &QPushButton::clicked, this, &OptionsDialog::onCancel);
    connect(btnApply_,  &QPushButton::clicked, this, &OptionsDialog::onApply);
    connect(btnReset_,  &QPushButton::clicked, this, &OptionsDialog::onResetDefaults);

    // 打开时全键快照：取消回滚的基线（含"应用过再取消"的会话预览回滚）
    for (const AppSettings::Spec& sp : AppSettings::specTable())
        snapshot_.insert(QLatin1String(sp.key), s_->value(QLatin1String(sp.key)));

    loadWidgets();
    loadDbPage();
}

// ---- 页面构建 ----

QWidget* OptionsDialog::formWrap(QWidget* inner) {
    auto* w = new QWidget;             // static：无 this；addWidget 时由页面栈接管父子关系
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->addWidget(inner);
    lay->addStretch(1);
    return w;
}

QWidget* OptionsDialog::buildGeneralPage() {
    auto* form = new QFormLayout();
    langBox_ = new QComboBox(this);
    addComboItem(langBox_, SketchCAD::I18n::t(tr("跟随系统")), QStringLiteral(""));
    addComboItem(langBox_, SketchCAD::I18n::t(QString::fromUtf8("简体中文")), QStringLiteral("zh-CN"));
    addComboItem(langBox_, QStringLiteral("English"), QStringLiteral("en"));
    form->addRow(SketchCAD::I18n::t(tr("界面语言：")), langBox_);
    recentSpin_ = new QSpinBox(this);
    recentSpin_->setRange(0, 20);
    recentSpin_->setSuffix(QStringLiteral(" 条"));
    recentSpin_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("「文件 > 最近打开」子菜单保留的条目数；0 表示关闭最近文件")));
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("最近文件数：")), recentSpin_);
    undoDepthSpin_ = new QSpinBox(this);
    undoDepthSpin_->setRange(1, 1000);
    undoDepthSpin_->setSuffix(QStringLiteral(" 步"));
    undoDepthSpin_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("Ctrl+Z 最多能回退多少步；超出时丢弃最旧的一步。\n调大更省心但占内存（每步都含一份图纸状态）。")));
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("撤销栈深度：")), undoDepthSpin_);
    chkUndoDelta_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("撤销差量化存储（关则回落全量模式）")), this);
    chkUndoDelta_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("开启：每步仅存「变更集 + 每 50 步一个全量检查点」，大幅降低撤销内存。\n关闭：每步存整张图纸快照（用于性能/行为对照，内存占用更高）。\n切换会清空当前撤销历史。")));
    form->addRow(chkUndoDelta_);
    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("语言切换立即生效，无需重启。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildDisplayPage() {
    auto* form = new QFormLayout();
    chkGrid_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("显示栅格（F7）")), this);
    form->addRow(chkGrid_);
    chkSnap_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("栅格吸附（F9）：坐标量化到步长整数倍")), this);
    form->addRow(chkSnap_);
    stepSpin_ = new QDoubleSpinBox(this);
    stepSpin_->setRange(0.001, 1000000.0);
    stepSpin_->setDecimals(3);
    stepSpin_->setSuffix(QStringLiteral(" mm"));
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("吸附/栅格步长：")), stepSpin_);
    chkOrtho_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("正交模式（F8）：落点锁定水平/垂直")), this);
    form->addRow(chkOrtho_);
    chkDyn_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("动态输入 DYN：光标旁实时坐标提示")), this);
    form->addRow(chkDyn_);
    chkLwd_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("显示线宽（LWDISPLAY）")), this);
    form->addRow(chkLwd_);
    auto* sep = new QLabel(this);
    sep->setFrameShape(QFrame::HLine);
    form->addRow(sep);
    chkGpu_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("启用 GPU 加速渲染（预留）")), this);
    chkGpu_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("渲染后端消费此开关；当前版本仅记录偏好")));
    form->addRow(chkGpu_);
    chkOff_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("允许离屏渲染回退（预留）")), this);
    chkOff_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("GPU 不可用时回退到离屏渲染；当前版本仅记录偏好")));
    form->addRow(chkOff_);
    // 界面主题（内置亮/暗切换）：跟随系统 / 亮色 / 暗色
    auto* sepTheme = new QLabel(this);
    sepTheme->setFrameShape(QFrame::HLine);
    form->addRow(sepTheme);
    themeBox_ = new QComboBox(this);
    addComboItem(themeBox_, SketchCAD::I18n::t(tr("跟随系统")), QStringLiteral("system"));
    addComboItem(themeBox_, SketchCAD::I18n::t(tr("亮色（默认）")), QStringLiteral("light"));
    addComboItem(themeBox_, SketchCAD::I18n::t(tr("暗色")), QStringLiteral("dark"));
    form->addRow(SketchCAD::I18n::t(tr("界面主题：")), themeBox_);
    auto* themeHint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("切换后立即生效；「跟随系统」会随 Windows 深浅色自动变化。")), this);
    themeHint->setStyleSheet(QStringLiteral("color: gray;"));
    themeHint->setWordWrap(true);
    form->addRow(QString(), themeHint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildFilesPage() {
    auto* form = new QFormLayout();
    // 默认存盘目录
    saveDirEdit_ = new QLineEdit(this);
    saveDirEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("（空 = 使用系统默认/上次目录）")));
    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(saveDirEdit_, 1);
    auto* dirBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("浏览...")), this);
    dirRow->addWidget(dirBtn);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("默认存盘目录：")), dirRow);
    connect(dirBtn, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(
            this, SketchCAD::I18n::t(QString::fromUtf8("选择默认存盘目录")), saveDirEdit_->text());
        if (!d.isEmpty()) saveDirEdit_->setText(d);
    });
    // 默认模板
    tmplEdit_ = new QLineEdit(this);
    tmplEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("（空 = 新建时生成标准图框）")));
    auto* tplRow = new QHBoxLayout();
    tplRow->addWidget(tmplEdit_, 1);
    auto* tplBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("浏览...")), this);
    tplRow->addWidget(tplBtn);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("默认模板文件：")), tplRow);
    connect(tplBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, SketchCAD::I18n::t(QString::fromUtf8("选择默认模板")),
            tmplEdit_->text(),
            SketchCAD::I18n::t(QString::fromUtf8("草图与图形 (*.json *.dxf *.dwg);;所有文件 (*)")));
        if (!f.isEmpty()) tmplEdit_->setText(f);
    });
    // DWG 转换组件路径（覆盖 SKETCHCAD_DWGTOOLS）：读取/写出 R2000+ DWG 依赖
    // 外部 LibreDWG 转换器（dwg2dxf / dwgwrite）。空 = 用程序目录 tools/dwg 或环境变量。
    dwgToolsEdit_ = new QLineEdit(this);
    dwgToolsEdit_->setPlaceholderText(
        SketchCAD::I18n::t(QString::fromUtf8("（空 = 使用程序目录 tools/dwg 或环境变量 SKETCHCAD_DWGTOOLS）")));
    auto* dwgRow = new QHBoxLayout();
    dwgRow->addWidget(dwgToolsEdit_, 1);
    auto* dwgBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("浏览...")), this);
    dwgRow->addWidget(dwgBtn);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("DWG 转换组件路径：")), dwgRow);
    connect(dwgBtn, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(
            this, SketchCAD::I18n::t(QString::fromUtf8("选择含 dwg2dxf.exe / dwgwrite.exe 的目录")), dwgToolsEdit_->text());
        if (!d.isEmpty()) dwgToolsEdit_->setText(d);
    });
    auto* dwgHint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("读取/写出 R2000+ DWG 需要 LibreDWG 转换组件（dwg2dxf / dwgwrite，GPLv3，以独立进程运行，主程序不链接其代码）。\n安装包已在 tools/dwg/ 内置；如需自定义位置，在此指定（覆盖环境变量 SKETCHCAD_DWGTOOLS）。\n留空则自动查找程序目录 tools/dwg/ 或环境变量。")), this);
    dwgHint->setStyleSheet(QStringLiteral("color: gray;"));
    dwgHint->setWordWrap(true);
    form->addRow(QString(), dwgHint);
    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("打开/保存对话框将优先定位到默认存盘目录；\n最近使用过的目录会自动记忆（不属于图形内容，不随图存档）。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildAutosavePage() {
    auto* form = new QFormLayout();
    intervalBox_ = new QComboBox(this);
    addComboItem(intervalBox_, SketchCAD::I18n::t(QString::fromUtf8("关闭")), 0);
    addComboItem(intervalBox_, SketchCAD::I18n::t(QString::fromUtf8("5 分钟")), 5);
    addComboItem(intervalBox_, SketchCAD::I18n::t(QString::fromUtf8("10 分钟")), 10);
    addComboItem(intervalBox_, SketchCAD::I18n::t(QString::fromUtf8("15 分钟")), 15);
    addComboItem(intervalBox_, SketchCAD::I18n::t(QString::fromUtf8("30 分钟")), 30);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("自动保存间隔：")), intervalBox_);
    backupSpin_ = new QSpinBox(this);
    backupSpin_->setRange(0, 9);
    backupSpin_->setSuffix(QStringLiteral(" 份"));
    backupSpin_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("另存/覆盖时保留的历史备份（.bak）份数；0 = 不保留")));
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("备份保留份数：")), backupSpin_);
    diskSpin_ = new QSpinBox(this);
    diskSpin_->setRange(1, 1000000);
    diskSpin_->setSuffix(QStringLiteral(" MB"));
    diskSpin_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("磁盘剩余空间低于该值时，自动保存前提前告警")));
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("磁盘告警阈值：")), diskSpin_);
    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("快照保存在 %LOCALAPPDATA%/SketchCAD/autosave，异常退出后可从启动恢复对话框找回。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildPrintPage() {
    auto* form = new QFormLayout();
    printerBox_ = new QComboBox(this);
    addComboItem(printerBox_, SketchCAD::I18n::t(QString::fromUtf8("〈系统默认打印机〉")), QString());
    const QStringList names = QPrinterInfo::availablePrinterNames();
    for (const QString& n : names)
        addComboItem(printerBox_, n, n);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("默认打印机：")), printerBox_);
    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("「直接打印」优先使用此设备（设备不存在时回落系统默认）。\n纸张/方向/边距/打印样式等页面细节在「打印 → 打印设置...」中配置，\n退出程序时自动记住，无需在此重复设置。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildUserPage() {
    auto* form = new QFormLayout();
    userNameEdit_ = new QLineEdit(this);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("姓名：")), userNameEdit_);
    companyEdit_ = new QLineEdit(this);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("公司/单位：")), companyEdit_);
    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("用户信息用于标题栏「设计人/单位」栏的默认值（应用习惯，不随图存档）。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildPrivacyPage() {
    auto* form = new QFormLayout();
    chkTelemetry_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("启用匿名遥测（默认关闭）")), this);
    chkTelemetry_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("开启后仅记录匿名会话/崩溃计数（无任何图纸内容）。\n默认关闭；当前版本不含任何网络上报实现，开启也不会产生网络请求。")));
    form->addRow(chkTelemetry_);
    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("本程序默认不上传任何数据，没有任何后台网络请求。\n「导出诊断包」（帮助菜单 / 异常退出提示）只在本机生成 ZIP 文件，\n包内不含图纸几何与文字内容，是否发送完全由您决定。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildDatabasePage() {
    auto* form = new QFormLayout();

    // 当前连接状态（查看当前连接信息）
    dbStatusLabel_ = new QLabel(this);
    dbStatusLabel_->setWordWrap(true);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("当前状态：")), dbStatusLabel_);

    // 数据库类型
    dbTypeBox_ = new QComboBox(this);
    dbTypeBox_->addItem(SketchCAD::I18n::t(tr("SQLite（文件型，零配置）")), QStringLiteral("QSQLITE"));
    dbTypeBox_->addItem(SketchCAD::I18n::t(tr("MySQL（服务器型）")), QStringLiteral("QMYSQL"));
    // 当前 Qt 构建可能缺失 QMYSQL 插件：禁用该选项并给出提示，避免误选后出现硬编码驱动错误。
    const bool mysqlOk = QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"));
    if (!mysqlOk) {
        const int idx = dbTypeBox_->findData(QStringLiteral("QMYSQL"));
        if (idx >= 0) {
            if (auto* m = qobject_cast<QStandardItemModel*>(dbTypeBox_->model())) {
                if (QStandardItem* item = m->item(idx)) item->setEnabled(false);
            }
        }
    }
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("数据库类型：")), dbTypeBox_);

    dbDriverHintLabel_ = new QLabel(
        SketchCAD::I18n::t(tr("⚠ MySQL 驱动不可用：当前环境缺少 Qt 的 QMYSQL 插件（qsqlmysql.dll 与 libmysql.dll）。\n"
           "已禁用 MySQL 连接，数据库功能默认使用内置 SQLite，可正常工作。\n"
           "如需启用 MySQL，请将 qsqlmysql.dll 放入程序目录的 sqldrivers/ 子目录并配置 libmysql.dll 路径后重启。")), this);
    dbDriverHintLabel_->setWordWrap(true);
    dbDriverHintLabel_->setStyleSheet(QStringLiteral("color: #c8761a;"));
    dbDriverHintLabel_->setVisible(!mysqlOk);
    form->addRow(QString(), dbDriverHintLabel_);

    // SQLite 设置
    dbSqliteGroup_ = new QGroupBox(SketchCAD::I18n::t(tr("SQLite 设置")), this);
    auto* sf = new QFormLayout(dbSqliteGroup_);
    dbFileEdit_ = new QLineEdit(dbSqliteGroup_);
    dbFileEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("留空则使用默认：~/SketchCAD/sketchcad_drawings.db")));
    auto* fileRow = new QHBoxLayout();
    fileRow->addWidget(dbFileEdit_, 1);
    auto* fileBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("浏览...")), dbSqliteGroup_);
    fileRow->addWidget(fileBtn);
    sf->addRow(SketchCAD::I18n::t(QString::fromUtf8("数据库文件：")), fileRow);

    // MySQL 设置
    dbMysqlGroup_ = new QGroupBox(SketchCAD::I18n::t(tr("MySQL 设置")), this);
    auto* mf = new QFormLayout(dbMysqlGroup_);
    dbHostEdit_ = new QLineEdit(dbMysqlGroup_);
    dbPortEdit_ = new QLineEdit(dbMysqlGroup_);
    dbPortEdit_->setFixedWidth(90);
    dbPortEdit_->setText(QStringLiteral("3306"));
    dbNameEdit_ = new QLineEdit(dbMysqlGroup_);
    dbUserEdit_ = new QLineEdit(dbMysqlGroup_);
    dbPassEdit_ = new QLineEdit(dbMysqlGroup_);
    dbPassEdit_->setEchoMode(QLineEdit::Password);
    mf->addRow(SketchCAD::I18n::t(QString::fromUtf8("主机：")), dbHostEdit_);
    mf->addRow(SketchCAD::I18n::t(QString::fromUtf8("端口：")), dbPortEdit_);
    mf->addRow(SketchCAD::I18n::t(QString::fromUtf8("数据库名：")), dbNameEdit_);
    mf->addRow(SketchCAD::I18n::t(QString::fromUtf8("用户名：")), dbUserEdit_);
    mf->addRow(SketchCAD::I18n::t(QString::fromUtf8("密码：")), dbPassEdit_);

    form->addRow(dbSqliteGroup_);
    form->addRow(dbMysqlGroup_);

    // 操作按钮
    auto* btnRow = new QHBoxLayout();
    auto* testBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("测试连接")), this);
    auto* saveBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("保存并连接")), this);
    btnRow->addWidget(testBtn);
    btnRow->addWidget(saveBtn);
    btnRow->addStretch(1);
    form->addRow(btnRow);

    connect(dbTypeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OptionsDialog::onDbDriverChanged);
    connect(fileBtn, &QPushButton::clicked, this, &OptionsDialog::onDbBrowse);
    connect(testBtn, &QPushButton::clicked, this, &OptionsDialog::onDbTest);
    connect(saveBtn,  &QPushButton::clicked, this, &OptionsDialog::onDbSave);

    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("数据库连接信息保存在本地配置文件 ~/SketchCAD/db_config.json，程序启动即按此连接；修改后点击「保存并连接」立即生效（无需关闭本对话框）。未连接时程序自动使用文件存储，不影响绘图。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);

    onDbDriverChanged(dbTypeBox_->currentIndex());   // 初始化可见性

    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildAiPage() {
    auto* form = new QFormLayout();

    aiEnabledBox_ = new QComboBox(this);
    addComboItem(aiEnabledBox_, SketchCAD::I18n::t(tr("启用")), true);
    addComboItem(aiEnabledBox_, SketchCAD::I18n::t(tr("停用")), false);
    form->addRow(SketchCAD::I18n::t(tr("AI 助手总开关：")), aiEnabledBox_);

    aiProviderBox_ = new QComboBox(this);
    addComboItem(aiProviderBox_, SketchCAD::I18n::t(tr("云端（OpenAI 兼容：DeepSeek/通义/智谱…）")), QStringLiteral("cloud"));
    addComboItem(aiProviderBox_, SketchCAD::I18n::t(tr("本地（Ollama / llama.cpp 等本地服务）")),    QStringLiteral("local"));
    addComboItem(aiProviderBox_, SketchCAD::I18n::t(tr("离线模拟（无需网络/密钥，规则联调）")),     QStringLiteral("mock"));
    form->addRow(SketchCAD::I18n::t(tr("后端类型：")), aiProviderBox_);

    aiBaseUrlEdit_ = new QLineEdit(this);
    aiBaseUrlEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("如 https://api.deepseek.com/v1（会自动追加 /chat/completions）")));
    aiBaseUrlEdit_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("必须与「API 密钥」所属厂商一致，否则会返回 HTTP 401。\nDeepSeek：https://api.deepseek.com 或 https://api.deepseek.com/v1\nOpenAI：https://api.openai.com/v1\n本地 Ollama：http://localhost:11434/v1\n本程序会在此地址后自动追加 /chat/completions，请勿重复填写该路径。")));
    auto* urlRow = new QHBoxLayout();
    urlRow->addWidget(aiBaseUrlEdit_, 1);
    form->addRow(SketchCAD::I18n::t(tr("服务地址：")), urlRow);

    aiApiKeyEdit_ = new QLineEdit(this);
    aiApiKeyEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("云端后端需要；本地/离线可为空")));
    aiApiKeyEdit_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("只填密钥本身，不要把接口 URL 填到这里。云端后端留空会返回 HTTP 401。\nDeepSeek 密钥在 https://platform.deepseek.com/api_keys 创建。")));
    aiApiKeyEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(SketchCAD::I18n::t(tr("API 密钥：")), aiApiKeyEdit_);

    aiModelEdit_ = new QLineEdit(this);
    aiModelEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("模型名，如 deepseek-chat / gpt-4o-mini / qwen-plus")));
    aiModelEdit_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("填该服务实际提供的模型 id。DeepSeek 常用 deepseek-chat（非思考）/ deepseek-reasoner（思考），具体以厂商控制台当前可用模型为准。")));
    form->addRow(SketchCAD::I18n::t(tr("模型：")), aiModelEdit_);

    aiMaxTokensSpin_ = new QSpinBox(this);
    aiMaxTokensSpin_->setRange(64, 8192);
    aiMaxTokensSpin_->setSuffix(QStringLiteral(" token"));
    form->addRow(SketchCAD::I18n::t(tr("单次回复上限：")), aiMaxTokensSpin_);

    aiAutoExecChk_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("非破坏性命令自动执行（关闭则需逐条确认）")), this);
    aiAutoExecChk_->setToolTip(SketchCAD::I18n::t(QString::fromUtf8("开启：画圆/直线等不会破坏现有图形的命令，助手直接执行。\n关闭：每条命令执行前都先展示，由你确认。\n破坏性命令（删除/修剪/分解…）无论此项如何，都需你手动确认。")));
    form->addRow(aiAutoExecChk_);

    auto* hint = new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("自然语言助手会复用现有命令体系（与命令行同一套语法）执行你的意图。\n云服务会把你的绘图指令文本发送给所选 LLM 服务商；离线模拟（mock）完全在本机运行、不上传任何数据。\n修改「后端类型 / 服务地址 / 密钥 / 模型」后点「应用」或「确定」立即生效。")), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);

    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

void OptionsDialog::loadDbPage() {
    DrawingDb* db = DrawingDb::instance();
    int idx = dbTypeBox_->findData(db->driver());
    if (idx < 0) idx = 0;
    // 已保存配置若是 QMYSQL 但当前驱动缺失，则回落 SQLite，避免选中被禁用的项。
    if (db->driver() == QStringLiteral("QMYSQL") && !QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL")))
        idx = 0;
    dbTypeBox_->setCurrentIndex(idx);
    dbFileEdit_->setText(db->dbPath());
    dbHostEdit_->setText(db->host());
    dbPortEdit_->setText(QString::number(db->port()));
    dbNameEdit_->setText(db->dbName());
    dbUserEdit_->setText(db->user());
    dbPassEdit_->setText(db->pass());
    onDbDriverChanged(idx);
    refreshDbStatus();
}

void OptionsDialog::refreshDbStatus() {
    DrawingDb* db = DrawingDb::instance();
    if (db->isConnected()) {
        const QString kind = db->driver() == QStringLiteral("QSQLITE")
            ? QStringLiteral("SQLite 文件型") : QStringLiteral("MySQL 服务器型");
        dbStatusLabel_->setText(SketchCAD::I18n::t(QString::fromUtf8("已连接（%1）")).arg(kind));
        dbStatusLabel_->setStyleSheet(QStringLiteral("color: #2e8b57; font-weight: bold;"));
    } else {
        dbStatusLabel_->setText(SketchCAD::I18n::t(QString::fromUtf8("未连接（使用文件存储）")));
        dbStatusLabel_->setStyleSheet(QStringLiteral("color: #c8761a; font-weight: bold;"));
    }
}

void OptionsDialog::onDbDriverChanged(int index) {
    const bool isSqlite = dbTypeBox_->itemData(index).toString() == QStringLiteral("QSQLITE");
    dbSqliteGroup_->setVisible(isSqlite);
    dbMysqlGroup_->setVisible(!isSqlite);
}

void OptionsDialog::onDbBrowse() {
    const QString cur = dbFileEdit_->text().isEmpty() ? QDir::homePath() : dbFileEdit_->text();
    const QString path = QFileDialog::getSaveFileName(this, SketchCAD::I18n::t(tr("选择 SQLite 数据库文件")),
        cur, SketchCAD::I18n::t(tr("SQLite 数据库 (*.db *.sqlite);;所有文件 (*.*)")));
    if (!path.isEmpty()) dbFileEdit_->setText(path);
}

void OptionsDialog::onDbTest() {
    const QString driver = dbTypeBox_->currentData().toString();
    const QString dbPath = dbFileEdit_->text().trimmed();
    const QString host   = dbHostEdit_->text().trimmed();
    const int     port   = dbPortEdit_->text().toInt();
    const QString dbName = dbNameEdit_->text().trimmed();
    const QString user   = dbUserEdit_->text();
    const QString pass   = dbPassEdit_->text();
    const QString err = DrawingDb::testConnection(driver, dbPath, host, port, dbName, user, pass);
    if (err.isEmpty())
        QMessageBox::information(this, SketchCAD::I18n::t(tr("测试连接")), SketchCAD::I18n::t(tr("连接成功。")));
    else
        QMessageBox::warning(this, SketchCAD::I18n::t(tr("测试连接")), SketchCAD::I18n::t(tr("连接失败：\n%1")).arg(err));
}

void OptionsDialog::onDbSave() {
    DrawingDb* db = DrawingDb::instance();
    const QString driver = dbTypeBox_->currentData().toString();
    // 防御：驱动缺失时直接提示并放弃保存，避免写入不可用的连接配置。
    if (!QSqlDatabase::isDriverAvailable(driver)) {
        QMessageBox::warning(this, SketchCAD::I18n::t(tr("保存并连接")),
            SketchCAD::I18n::t(tr("当前 Qt 构建未包含该数据库驱动（%1），无法保存并连接。\n请改用内置的 SQLite，或将对应驱动插件放入程序目录的 sqldrivers/ 子目录后重启。")).arg(driver));
        return;
    }
    const QString dbPath = dbFileEdit_->text().trimmed();
    const QString host   = dbHostEdit_->text().trimmed();
    const int     port   = dbPortEdit_->text().toInt();
    const QString dbName = dbNameEdit_->text().trimmed();
    const QString user   = dbUserEdit_->text();
    const QString pass   = dbPassEdit_->text();

    db->closeConnection();
    db->setConnectionInfo(driver, dbPath, host, dbName, user, pass);
    db->setPort(port);
    if (!db->saveConfigToFile()) {
        QMessageBox::warning(this, SketchCAD::I18n::t(tr("数据库配置")),
            SketchCAD::I18n::t(tr("配置已应用，但写入配置文件失败：%1\n下次启动可能回落默认连接。")).arg(db->lastError()));
    }
    db->connect();
    refreshDbStatus();
    emit databaseConfigChanged();   // 通知主窗口刷新状态栏 / 菜单可用态
}

void OptionsDialog::addPage(const QString& title, QWidget* page) {
    new QTreeWidgetItem(catTree_, QStringList{ title });   // 以 catTree_ 为父 → 自动成为顶层项
    pages_->addWidget(page);
}

// ---- 控件 <-> 设置 ----

void OptionsDialog::loadWidgets() {
    langBox_->setCurrentIndex(comboIndexOfData(langBox_, s_->language()));
    recentSpin_->setValue(s_->recentFileCount());
    undoDepthSpin_->setValue(s_->undoDepth());
    chkUndoDelta_->setChecked(s_->undoDelta());
    chkGrid_->setChecked(s_->showGrid());
    chkSnap_->setChecked(s_->gridSnap());
    stepSpin_->setValue(s_->gridSnapStep());
    chkOrtho_->setChecked(s_->orthoMode());
    chkDyn_->setChecked(s_->dynMode());
    chkLwd_->setChecked(s_->showLineWeight());
    chkGpu_->setChecked(s_->gpuAcceleration());
    chkOff_->setChecked(s_->offscreenFallback());
    themeBox_->setCurrentIndex(comboIndexOfData(themeBox_, s_->theme()));
    saveDirEdit_->setText(s_->defaultSaveDir());
    tmplEdit_->setText(s_->defaultTemplate());
    dwgToolsEdit_->setText(s_->dwgToolsOverride());
    intervalBox_->setCurrentIndex(comboIndexOfData(intervalBox_, s_->autosaveIntervalMinutes()));
    backupSpin_->setValue(s_->backupCount());
    diskSpin_->setValue(s_->lowDiskThresholdMb());
    printerBox_->setCurrentIndex(comboIndexOfData(printerBox_, s_->printerName()));
    userNameEdit_->setText(s_->userName());
    companyEdit_->setText(s_->userCompany());
    chkTelemetry_->setChecked(s_->telemetryEnabled());
    // AI 自然语言助手
    aiEnabledBox_->setCurrentIndex(comboIndexOfData(aiEnabledBox_, s_->aiEnabled()));
    aiProviderBox_->setCurrentIndex(comboIndexOfData(aiProviderBox_, s_->aiProvider()));
    aiBaseUrlEdit_->setText(s_->aiBaseUrl());
    aiApiKeyEdit_->setText(s_->aiApiKey());
    aiModelEdit_->setText(s_->aiModel());
    aiMaxTokensSpin_->setValue(s_->aiMaxTokens());
    aiAutoExecChk_->setChecked(s_->aiAutoExecute());
}

void OptionsDialog::loadWidgetsFromDefaults() {
    // 从规格表取默认值回填控件（草稿态；须再点确定/应用才生效）
    langBox_->setCurrentIndex(comboIndexOfData(langBox_, AppSettings::defaultValue(QLatin1String(kLanguage))));
    recentSpin_->setValue(AppSettings::defaultValue(QLatin1String(kRecentFileCount)).toInt());
    undoDepthSpin_->setValue(AppSettings::defaultValue(QLatin1String(kUndoDepth)).toInt());
    chkUndoDelta_->setChecked(AppSettings::defaultValue(QLatin1String(kUndoDelta)).toBool());
    chkGrid_->setChecked(AppSettings::defaultValue(QLatin1String(kShowGrid)).toBool());
    chkSnap_->setChecked(AppSettings::defaultValue(QLatin1String(kGridSnap)).toBool());
    stepSpin_->setValue(AppSettings::defaultValue(QLatin1String(kGridSnapStep)).toDouble());
    chkOrtho_->setChecked(AppSettings::defaultValue(QLatin1String(kOrtho)).toBool());
    chkDyn_->setChecked(AppSettings::defaultValue(QLatin1String(kDynMode)).toBool());
    chkLwd_->setChecked(AppSettings::defaultValue(QLatin1String(kShowLineWeight)).toBool());
    chkGpu_->setChecked(AppSettings::defaultValue(QLatin1String(kGpuAcceleration)).toBool());
    chkOff_->setChecked(AppSettings::defaultValue(QLatin1String(kOffscreenFallback)).toBool());
    themeBox_->setCurrentIndex(comboIndexOfData(themeBox_, AppSettings::defaultValue(QLatin1String(kTheme))));
    saveDirEdit_->setText(AppSettings::defaultValue(QLatin1String(kDefaultSaveDir)).toString());
    tmplEdit_->setText(AppSettings::defaultValue(QLatin1String(kDefaultTemplate)).toString());
    dwgToolsEdit_->setText(AppSettings::defaultValue(QLatin1String(kDwgToolsOverride)).toString());
    intervalBox_->setCurrentIndex(
        comboIndexOfData(intervalBox_, AppSettings::defaultValue(QLatin1String(kAutosaveInterval))));
    backupSpin_->setValue(AppSettings::defaultValue(QLatin1String(kBackupCount)).toInt());
    diskSpin_->setValue(AppSettings::defaultValue(QLatin1String(kLowDiskMb)).toInt());
    printerBox_->setCurrentIndex(comboIndexOfData(printerBox_, QString()));
    userNameEdit_->setText(AppSettings::defaultValue(QLatin1String(kUserName)).toString());
    companyEdit_->setText(AppSettings::defaultValue(QLatin1String(kUserCompany)).toString());
    chkTelemetry_->setChecked(AppSettings::defaultValue(QLatin1String(kTelemetryEnabled)).toBool());
    // AI 自然语言助手（默认值回填草稿；须点应用/确定才生效）
    aiEnabledBox_->setCurrentIndex(comboIndexOfData(aiEnabledBox_, AppSettings::defaultValue(QLatin1String(kAiEnabled)).toBool()));
    aiProviderBox_->setCurrentIndex(comboIndexOfData(aiProviderBox_, AppSettings::defaultValue(QLatin1String(kAiProvider)).toString()));
    aiBaseUrlEdit_->setText(AppSettings::defaultValue(QLatin1String(kAiBaseUrl)).toString());
    aiApiKeyEdit_->setText(AppSettings::defaultValue(QLatin1String(kAiApiKey)).toString());
    aiModelEdit_->setText(AppSettings::defaultValue(QLatin1String(kAiModel)).toString());
    aiMaxTokensSpin_->setValue(AppSettings::defaultValue(QLatin1String(kAiMaxTokens)).toInt());
    aiAutoExecChk_->setChecked(AppSettings::defaultValue(QLatin1String(kAiAutoExecute)).toBool());
}

void OptionsDialog::collectWidgets() {
    s_->setLanguage(langBox_->currentData().toString());
    s_->setRecentFileCount(recentSpin_->value());
    s_->setUndoDepth(undoDepthSpin_->value());
    s_->setUndoDelta(chkUndoDelta_->isChecked());
    s_->setShowGrid(chkGrid_->isChecked());
    s_->setGridSnap(chkSnap_->isChecked());
    s_->setGridSnapStep(stepSpin_->value());
    s_->setOrthoMode(chkOrtho_->isChecked());
    s_->setDynMode(chkDyn_->isChecked());
    s_->setShowLineWeight(chkLwd_->isChecked());
    s_->setGpuAcceleration(chkGpu_->isChecked());
    s_->setOffscreenFallback(chkOff_->isChecked());
    s_->setTheme(themeBox_->currentData().toString());
    s_->setDefaultSaveDir(saveDirEdit_->text().trimmed());
    s_->setDefaultTemplate(tmplEdit_->text().trimmed());
    s_->setDwgToolsOverride(dwgToolsEdit_->text().trimmed());
    s_->setAutosaveIntervalMinutes(intervalBox_->currentData().toInt());
    s_->setBackupCount(backupSpin_->value());
    s_->setLowDiskThresholdMb(diskSpin_->value());
    s_->setPrinterName(printerBox_->currentData().toString());
    s_->setUserName(userNameEdit_->text().trimmed());
    s_->setUserCompany(companyEdit_->text().trimmed());
    s_->setTelemetryEnabled(chkTelemetry_->isChecked());
    // AI 自然语言助手
    s_->setAiEnabled(aiEnabledBox_->currentData().toBool());
    s_->setAiProvider(aiProviderBox_->currentData().toString());
    s_->setAiBaseUrl(aiBaseUrlEdit_->text().trimmed());
    s_->setAiApiKey(aiApiKeyEdit_->text().trimmed());
    s_->setAiModel(aiModelEdit_->text().trimmed());
    s_->setAiMaxTokens(aiMaxTokensSpin_->value());
    s_->setAiAutoExecute(aiAutoExecChk_->isChecked());
}

// ---- 动作 ----

void OptionsDialog::warnWriteFailed() {
    QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("选项写入失败")),
        SketchCAD::I18n::t(QString::fromUtf8("%1\n\n本次改动可能在重启后丢失，请检查磁盘空间或以可写权限重新运行程序。"))
            .arg(s_->lastError()));
}

bool OptionsDialog::applyAndCheck() {
    collectWidgets();               // 逐键 setValue → settingChanged → 主窗口即时生效
    if (!s_->syncAndCheck()) {      // 写失败必须提示（禁止静默失败）
        warnWriteFailed();
        return false;
    }
    return true;
}

void OptionsDialog::onApply() {
    applyAndCheck();
}

void OptionsDialog::onCancel() {
    // 回滚本次会话内的全部即时预览改动（含已应用过的）：逐键写回打开时的快照。
    // 与预览走同一 settingChanged 信号通道，主窗口/模型同步回到原状态。
    for (auto it = snapshot_.constBegin(); it != snapshot_.constEnd(); ++it)
        if (s_->value(it.key()) != it.value())
            s_->setValue(it.key(), it.value());
    if (!s_->syncAndCheck()) warnWriteFailed();
    reject();
}

void OptionsDialog::onResetDefaults() {
    // 二次确认（需求 2）：误触不直接破坏当前配置
    const auto ret = QMessageBox::question(
        this, SketchCAD::I18n::t(QString::fromUtf8("重置为默认值")),
        SketchCAD::I18n::t(QString::fromUtf8("将把所有分类的设置恢复为出厂默认值。\n\n注意：点「是」后仍需点「应用」或「确定」才真正生效。\n是否继续？")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    loadWidgetsFromDefaults();
}

} // namespace SketchCAD
