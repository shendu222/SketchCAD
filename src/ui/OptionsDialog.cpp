#include "OptionsDialog.h"

#include "core/AppSettings.h"

#include <QTreeWidget>
#include <QStackedWidget>
#include <QComboBox>
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
    setWindowTitle(tr("选项"));
    setModal(true);
    resize(720, 540);

    auto* rootLay = new QVBoxLayout(this);

    // 左分类树 + 右页面栈
    auto* body = new QHBoxLayout();
    catTree_ = new QTreeWidget(this);
    catTree_->setFixedWidth(150);
    catTree_->setHeaderLabel(tr("分类"));
    catTree_->setRootIsDecorated(false);
    pages_ = new QStackedWidget(this);
    body->addWidget(catTree_);
    body->addWidget(pages_, 1);
    rootLay->addLayout(body, 1);

    // 底部按钮：重置（左）| stretch | 确定 取消 应用（右）
    btnReset_   = new QPushButton(tr("重置为默认值"), this);
    btnOk_      = new QPushButton(tr("确定"), this);
    btnCancel_  = new QPushButton(tr("取消"), this);
    btnApply_   = new QPushButton(tr("应用"), this);
    btnOk_->setDefault(true);
    auto* btnLay = new QHBoxLayout();
    btnLay->addWidget(btnReset_);
    btnLay->addStretch(1);
    btnLay->addWidget(btnOk_);
    btnLay->addWidget(btnCancel_);
    btnLay->addWidget(btnApply_);
    rootLay->addLayout(btnLay);

    // 七个分类页（顺序 = 需求规定）
    addPage(tr("常规"),     buildGeneralPage());
    addPage(tr("显示"),     buildDisplayPage());
    addPage(tr("文件位置"), buildFilesPage());
    addPage(tr("自动保存"), buildAutosavePage());
    addPage(tr("打印"),     buildPrintPage());
    addPage(tr("用户"),     buildUserPage());
    addPage(tr("隐私"),     buildPrivacyPage());

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
    addComboItem(langBox_, tr("跟随系统"), QStringLiteral(""));
    addComboItem(langBox_, QStringLiteral("简体中文"), QStringLiteral("zh-CN"));
    addComboItem(langBox_, QStringLiteral("English"), QStringLiteral("en"));
    form->addRow(tr("界面语言："), langBox_);
    recentSpin_ = new QSpinBox(this);
    recentSpin_->setRange(0, 20);
    recentSpin_->setSuffix(QStringLiteral(" 条"));
    recentSpin_->setToolTip(QStringLiteral("「文件 > 最近打开」子菜单保留的条目数；0 表示关闭最近文件"));
    form->addRow(QStringLiteral("最近文件数："), recentSpin_);
    undoDepthSpin_ = new QSpinBox(this);
    undoDepthSpin_->setRange(1, 1000);
    undoDepthSpin_->setSuffix(QStringLiteral(" 步"));
    undoDepthSpin_->setToolTip(QStringLiteral(
        "Ctrl+Z 最多能回退多少步；超出时丢弃最旧的一步。\n"
        "调大更省心但占内存（每步都含一份图纸状态）。"));
    form->addRow(QStringLiteral("撤销栈深度："), undoDepthSpin_);
    auto* hint = new QLabel(
        QStringLiteral("语言切换需要重新启动程序后完全生效。"), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildDisplayPage() {
    auto* form = new QFormLayout();
    chkGrid_ = new QCheckBox(QStringLiteral("显示栅格（F7）"), this);
    form->addRow(chkGrid_);
    chkSnap_ = new QCheckBox(QStringLiteral("栅格吸附（F9）：坐标量化到步长整数倍"), this);
    form->addRow(chkSnap_);
    stepSpin_ = new QDoubleSpinBox(this);
    stepSpin_->setRange(0.001, 1000000.0);
    stepSpin_->setDecimals(3);
    stepSpin_->setSuffix(QStringLiteral(" mm"));
    form->addRow(QStringLiteral("吸附/栅格步长："), stepSpin_);
    chkOrtho_ = new QCheckBox(QStringLiteral("正交模式（F8）：落点锁定水平/垂直"), this);
    form->addRow(chkOrtho_);
    chkDyn_ = new QCheckBox(QStringLiteral("动态输入 DYN：光标旁实时坐标提示"), this);
    form->addRow(chkDyn_);
    chkLwd_ = new QCheckBox(QStringLiteral("显示线宽（LWDISPLAY）"), this);
    form->addRow(chkLwd_);
    auto* sep = new QLabel(this);
    sep->setFrameShape(QFrame::HLine);
    form->addRow(sep);
    chkGpu_ = new QCheckBox(QStringLiteral("启用 GPU 加速渲染（预留）"), this);
    chkGpu_->setToolTip(QStringLiteral("渲染后端消费此开关；当前版本仅记录偏好"));
    form->addRow(chkGpu_);
    chkOff_ = new QCheckBox(QStringLiteral("允许离屏渲染回退（预留）"), this);
    chkOff_->setToolTip(QStringLiteral("GPU 不可用时回退到离屏渲染；当前版本仅记录偏好"));
    form->addRow(chkOff_);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildFilesPage() {
    auto* form = new QFormLayout();
    // 默认存盘目录
    saveDirEdit_ = new QLineEdit(this);
    saveDirEdit_->setPlaceholderText(QStringLiteral("（空 = 使用系统默认/上次目录）"));
    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(saveDirEdit_, 1);
    auto* dirBtn = new QPushButton(QStringLiteral("浏览..."), this);
    dirRow->addWidget(dirBtn);
    form->addRow(QStringLiteral("默认存盘目录："), dirRow);
    connect(dirBtn, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择默认存盘目录"), saveDirEdit_->text());
        if (!d.isEmpty()) saveDirEdit_->setText(d);
    });
    // 默认模板
    tmplEdit_ = new QLineEdit(this);
    tmplEdit_->setPlaceholderText(QStringLiteral("（空 = 新建时生成标准图框）"));
    auto* tplRow = new QHBoxLayout();
    tplRow->addWidget(tmplEdit_, 1);
    auto* tplBtn = new QPushButton(QStringLiteral("浏览..."), this);
    tplRow->addWidget(tplBtn);
    form->addRow(QStringLiteral("默认模板文件："), tplRow);
    connect(tplBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择默认模板"),
            tmplEdit_->text(),
            QStringLiteral("草图与图形 (*.json *.dxf *.dwg);;所有文件 (*)"));
        if (!f.isEmpty()) tmplEdit_->setText(f);
    });
    auto* hint = new QLabel(
        QStringLiteral("打开/保存对话框将优先定位到默认存盘目录；\n"
                       "最近使用过的目录会自动记忆（不属于图形内容，不随图存档）。"), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildAutosavePage() {
    auto* form = new QFormLayout();
    intervalBox_ = new QComboBox(this);
    addComboItem(intervalBox_, QStringLiteral("关闭"), 0);
    addComboItem(intervalBox_, QStringLiteral("5 分钟"), 5);
    addComboItem(intervalBox_, QStringLiteral("10 分钟"), 10);
    addComboItem(intervalBox_, QStringLiteral("15 分钟"), 15);
    addComboItem(intervalBox_, QStringLiteral("30 分钟"), 30);
    form->addRow(QStringLiteral("自动保存间隔："), intervalBox_);
    backupSpin_ = new QSpinBox(this);
    backupSpin_->setRange(0, 9);
    backupSpin_->setSuffix(QStringLiteral(" 份"));
    backupSpin_->setToolTip(QStringLiteral("另存/覆盖时保留的历史备份（.bak）份数；0 = 不保留"));
    form->addRow(QStringLiteral("备份保留份数："), backupSpin_);
    diskSpin_ = new QSpinBox(this);
    diskSpin_->setRange(1, 1000000);
    diskSpin_->setSuffix(QStringLiteral(" MB"));
    diskSpin_->setToolTip(QStringLiteral("磁盘剩余空间低于该值时，自动保存前提前告警"));
    form->addRow(QStringLiteral("磁盘告警阈值："), diskSpin_);
    auto* hint = new QLabel(
        QStringLiteral("快照保存在 %LOCALAPPDATA%/SketchCAD/autosave，异常退出后可从启动恢复对话框找回。"), this);
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
    addComboItem(printerBox_, QStringLiteral("〈系统默认打印机〉"), QString());
    const QStringList names = QPrinterInfo::availablePrinterNames();
    for (const QString& n : names)
        addComboItem(printerBox_, n, n);
    form->addRow(QStringLiteral("默认打印机："), printerBox_);
    auto* hint = new QLabel(
        QStringLiteral("「直接打印」优先使用此设备（设备不存在时回落系统默认）。\n"
                       "纸张/方向/边距/打印样式等页面细节在「打印 → 打印设置...」中配置，\n"
                       "退出程序时自动记住，无需在此重复设置。"), this);
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
    form->addRow(QStringLiteral("姓名："), userNameEdit_);
    companyEdit_ = new QLineEdit(this);
    form->addRow(QStringLiteral("公司/单位："), companyEdit_);
    auto* hint = new QLabel(
        QStringLiteral("用户信息用于标题栏「设计人/单位」栏的默认值（应用习惯，不随图存档）。"), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
}

QWidget* OptionsDialog::buildPrivacyPage() {
    auto* form = new QFormLayout();
    chkTelemetry_ = new QCheckBox(QStringLiteral("启用匿名遥测（默认关闭）"), this);
    chkTelemetry_->setToolTip(QStringLiteral(
        "开启后仅记录匿名会话/崩溃计数（无任何图纸内容）。\n"
        "默认关闭；当前版本不含任何网络上报实现，开启也不会产生网络请求。"));
    form->addRow(chkTelemetry_);
    auto* hint = new QLabel(
        QStringLiteral("本程序默认不上传任何数据，没有任何后台网络请求。\n"
                       "「导出诊断包」（帮助菜单 / 异常退出提示）只在本机生成 ZIP 文件，\n"
                       "包内不含图纸几何与文字内容，是否发送完全由您决定。"), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    form->addRow(QString(), hint);
    auto* inner = new QWidget(this);
    inner->setLayout(form);
    return formWrap(inner);
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
    chkGrid_->setChecked(s_->showGrid());
    chkSnap_->setChecked(s_->gridSnap());
    stepSpin_->setValue(s_->gridSnapStep());
    chkOrtho_->setChecked(s_->orthoMode());
    chkDyn_->setChecked(s_->dynMode());
    chkLwd_->setChecked(s_->showLineWeight());
    chkGpu_->setChecked(s_->gpuAcceleration());
    chkOff_->setChecked(s_->offscreenFallback());
    saveDirEdit_->setText(s_->defaultSaveDir());
    tmplEdit_->setText(s_->defaultTemplate());
    intervalBox_->setCurrentIndex(comboIndexOfData(intervalBox_, s_->autosaveIntervalMinutes()));
    backupSpin_->setValue(s_->backupCount());
    diskSpin_->setValue(s_->lowDiskThresholdMb());
    printerBox_->setCurrentIndex(comboIndexOfData(printerBox_, s_->printerName()));
    userNameEdit_->setText(s_->userName());
    companyEdit_->setText(s_->userCompany());
    chkTelemetry_->setChecked(s_->telemetryEnabled());
}

void OptionsDialog::loadWidgetsFromDefaults() {
    // 从规格表取默认值回填控件（草稿态；须再点确定/应用才生效）
    langBox_->setCurrentIndex(comboIndexOfData(langBox_, AppSettings::defaultValue(QLatin1String(kLanguage))));
    recentSpin_->setValue(AppSettings::defaultValue(QLatin1String(kRecentFileCount)).toInt());
    undoDepthSpin_->setValue(AppSettings::defaultValue(QLatin1String(kUndoDepth)).toInt());
    chkGrid_->setChecked(AppSettings::defaultValue(QLatin1String(kShowGrid)).toBool());
    chkSnap_->setChecked(AppSettings::defaultValue(QLatin1String(kGridSnap)).toBool());
    stepSpin_->setValue(AppSettings::defaultValue(QLatin1String(kGridSnapStep)).toDouble());
    chkOrtho_->setChecked(AppSettings::defaultValue(QLatin1String(kOrtho)).toBool());
    chkDyn_->setChecked(AppSettings::defaultValue(QLatin1String(kDynMode)).toBool());
    chkLwd_->setChecked(AppSettings::defaultValue(QLatin1String(kShowLineWeight)).toBool());
    chkGpu_->setChecked(AppSettings::defaultValue(QLatin1String(kGpuAcceleration)).toBool());
    chkOff_->setChecked(AppSettings::defaultValue(QLatin1String(kOffscreenFallback)).toBool());
    saveDirEdit_->setText(AppSettings::defaultValue(QLatin1String(kDefaultSaveDir)).toString());
    tmplEdit_->setText(AppSettings::defaultValue(QLatin1String(kDefaultTemplate)).toString());
    intervalBox_->setCurrentIndex(
        comboIndexOfData(intervalBox_, AppSettings::defaultValue(QLatin1String(kAutosaveInterval))));
    backupSpin_->setValue(AppSettings::defaultValue(QLatin1String(kBackupCount)).toInt());
    diskSpin_->setValue(AppSettings::defaultValue(QLatin1String(kLowDiskMb)).toInt());
    printerBox_->setCurrentIndex(comboIndexOfData(printerBox_, QString()));
    userNameEdit_->setText(AppSettings::defaultValue(QLatin1String(kUserName)).toString());
    companyEdit_->setText(AppSettings::defaultValue(QLatin1String(kUserCompany)).toString());
    chkTelemetry_->setChecked(AppSettings::defaultValue(QLatin1String(kTelemetryEnabled)).toBool());
}

void OptionsDialog::collectWidgets() {
    s_->setLanguage(langBox_->currentData().toString());
    s_->setRecentFileCount(recentSpin_->value());
    s_->setUndoDepth(undoDepthSpin_->value());
    s_->setShowGrid(chkGrid_->isChecked());
    s_->setGridSnap(chkSnap_->isChecked());
    s_->setGridSnapStep(stepSpin_->value());
    s_->setOrthoMode(chkOrtho_->isChecked());
    s_->setDynMode(chkDyn_->isChecked());
    s_->setShowLineWeight(chkLwd_->isChecked());
    s_->setGpuAcceleration(chkGpu_->isChecked());
    s_->setOffscreenFallback(chkOff_->isChecked());
    s_->setDefaultSaveDir(saveDirEdit_->text().trimmed());
    s_->setDefaultTemplate(tmplEdit_->text().trimmed());
    s_->setAutosaveIntervalMinutes(intervalBox_->currentData().toInt());
    s_->setBackupCount(backupSpin_->value());
    s_->setLowDiskThresholdMb(diskSpin_->value());
    s_->setPrinterName(printerBox_->currentData().toString());
    s_->setUserName(userNameEdit_->text().trimmed());
    s_->setUserCompany(companyEdit_->text().trimmed());
    s_->setTelemetryEnabled(chkTelemetry_->isChecked());
}

// ---- 动作 ----

void OptionsDialog::warnWriteFailed() {
    QMessageBox::warning(this, QStringLiteral("选项写入失败"),
        QStringLiteral("%1\n\n本次改动可能在重启后丢失，请检查磁盘空间或以可写权限重新运行程序。")
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
        this, QStringLiteral("重置为默认值"),
        QStringLiteral("将把所有分类的设置恢复为出厂默认值。\n\n"
                       "注意：点「是」后仍需点「应用」或「确定」才真正生效。\n是否继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    loadWidgetsFromDefaults();
}

} // namespace SketchCAD
