#pragma once
// 标准件原图模板库面板 —— 分类浏览 / 参数输入校验 / 实时预览 / 出图保存
// ---------------------------------------------------------------------------
// 左侧树按「类别 → 模板(规格)」列出数据/stdpart/templates 库；
// 右侧上为参数表单（QDoubleSpinBox，带范围校验与提示），
// 右侧中为 QPainter 实时预览（改任一参数即刻重渲染），
// 底部三个按钮：插入当前图纸 / 保存为图纸文件 / 重置为原规格。
#include <QWidget>
#include <QString>
#include <QVariantMap>
#include <vector>
#include "core/StdPartInstance.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QFormLayout;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;

namespace SketchCAD {
struct Element;
class SketchModel;

class StdPartTemplatePanel : public QWidget {
    Q_OBJECT
public:
    explicit StdPartTemplatePanel(SketchModel* model, QWidget* parent = nullptr);
    void refresh();

    // 测试接缝：当前渲染出的图元（替换占位符 + 几何缩放后，不含插入定位变换）
    const std::vector<Element>& rendered() const;
    bool loadStore(const QString& storeDir, QString* err = nullptr);

    // 当前表单状态 → 参数实例（供 MainWindow 插入/保存 JSON）
    QVariantMap parameters(QString* err = nullptr) const;
    StdPartPlacement placement() const;
    QString currentTemplateId() const;
    // 打开 JSON 后回填模板/参数/定位并刷新预览
    bool applyInstance(const StdPartInstance& inst, QString* err = nullptr);
    // 嵌入【标准件模板库】对话框时隐藏面板自带操作按钮，由对话框统一提供
    void setDialogMode(bool on);
    // 恢复当前模板的原规格参数（供对话框「新建」使用）
    void resetToDefaults();

signals:
    void insertRequested();            // 请求按当前定位设置把标准件插入图纸
    void saveRequested(const QString& path); // 请求保存 rendered() 到指定路径
    void saveJsonRequested(const QString& path); // 请求保存参数实例 JSON
    void openJsonRequested(const QString& path); // 请求打开参数实例 JSON

private slots:
    void onCategoryOrTemplatePicked(QTreeWidgetItem* item, int col);
    void onParamChanged();
    void onInsert();
    void onSave();
    void onReset();

private:
    void rebuildTree();
    void loadTemplate(const QString& id);
    void updatePreview();
    QVariantMap collectParams(QString* err = nullptr) const;
    QString proposeSavePath(const QString& ext = QStringLiteral("dxf")) const;

    SketchModel* model_;
    QTreeWidget* tree_ = nullptr;
    QWidget* previewHost_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QLabel* info_ = nullptr;
    QLabel* validation_ = nullptr;
    QLineEdit* specEdit_ = nullptr;
    QWidget* paramHost_ = nullptr;
    QFormLayout* paramForm_ = nullptr;
    QComboBox* insertModeCombo_ = nullptr;
    QComboBox* baseCombo_ = nullptr;
    QLineEdit* pointEdit_ = nullptr;
    QLineEdit* customBaseEdit_ = nullptr;
    QLabel* pickHint_ = nullptr;
    QDoubleSpinBox* angleSpin_ = nullptr;
    QLineEdit* layerEdit_ = nullptr;
    QPushButton* insertBtn_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QPushButton* resetBtn_ = nullptr;
    QPushButton* saveJsonBtn_ = nullptr;
    QPushButton* openJsonBtn_ = nullptr;

    QString storeDir_;
    QString currentId_;
    QVector<QWidget*> paramWidgets_;
    QStringList paramKeys_;
    std::vector<Element> rendered_;
};

} // namespace SketchCAD
