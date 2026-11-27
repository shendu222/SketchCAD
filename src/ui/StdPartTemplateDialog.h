#pragma once
// 【绘图 > 标准件】弹出的【标准件模板库】对话框。
// 原停靠面板的全部操作集中到本对话框：分类导航、缩略预览、参数配置、
// 实例工作集（新建/编辑/删除/导入/导出）以及应用/插入。
#include <QDialog>
#include <QVector>
#include <QString>
#include "core/StdPartInstance.h"

class QComboBox;
class QPushButton;
class QLabel;
class QGroupBox;

namespace SketchCAD {

class SketchModel;
class StdPartTemplatePanel;

class StdPartTemplateDialog : public QDialog {
    Q_OBJECT
public:
    explicit StdPartTemplateDialog(SketchModel* model, QWidget* parent = nullptr);

    StdPartTemplatePanel* panel() const { return panel_; }
    // 当前工作集实例（未新建时按面板状态即时组装）
    StdPartInstance currentInstance(QString* err = nullptr) const;
    int instanceCount() const { return instances_.size(); }

signals:
    void insertRequested();                       // 请求把当前实例插入图纸（由 MainWindow 处理落点）
    void instanceApplied(const StdPartInstance& inst); // 应用/导入后回显给主窗口

private slots:
    void onNew();
    void onEdit();
    void onDelete();
    void onImport();
    void onExport();
    void onApply();
    void onInstanceSelected(int index);
    void onPanelInsertRequested();

private:
    StdPartInstance makeInstanceFromPanel(QString* err) const;
    QString instanceLabel(const StdPartInstance& inst) const;
    int currentIndex() const;
    void rebuildCombo(int selectIndex = -1);
    void setStatus(const QString& text);

    SketchModel* model_ = nullptr;
    StdPartTemplatePanel* panel_ = nullptr;
    QComboBox* instanceCombo_ = nullptr;
    QPushButton* newBtn_ = nullptr;
    QPushButton* editBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* importBtn_ = nullptr;
    QPushButton* exportBtn_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QPushButton* insertBtn_ = nullptr;
    QLabel* status_ = nullptr;
    QVector<StdPartInstance> instances_;
};

} // namespace SketchCAD
