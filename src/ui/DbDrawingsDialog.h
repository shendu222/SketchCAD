#pragma once
#include <QDialog>

class QTabWidget;
class QTableWidget;
class QPushButton;

namespace SketchCAD {

// 「数据库」绘图记录 / 模板 表格对话框
//
// 仅做两件事：
//  1) 从 DrawingDb 读取并展示「图档」「模板」两张表（含操作列）；
//  2) 把用户的意图通过信号转发给 MainWindow（打开/应用/保存），由 MainWindow
//     复用既有 toDrawingJson()/readDrawing()/setScaleRatio() 完成真正的模型操作，
//     本对话框不触碰任何渲染或模型逻辑（零回归风险）。
// 删除操作是纯数据库行为，对话框直接调用 DrawingDb 完成并刷新自身。
class DbDrawingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DbDrawingsDialog(QWidget* parent = nullptr);

    // 重新从数据库拉取两张表（保存/删除后、对话框每次 show 时调用）
    void refresh();

signals:
    // 请求打开某个图档（id 为 drawings 表主键）
    void openDrawingRequested(int id);
    // 请求套用某个模板（id 为 templates 表主键；尺寸在 MainWindow 侧交互确认）
    void applyTemplateRequested(int id);
    // 请求将当前图纸按给定名称存入数据库
    void saveCurrentRequested(const QString& name);
    // 请求将当前图纸按给定名称存为模板
    void saveTemplateRequested(const QString& name);

private slots:
    void onSaveCurrent();
    void onSaveTemplate();

private:
    void buildDrawingsTable();
    void buildTemplatesTable();
    void rebuildDrawings();
    void rebuildTemplates();

    QTabWidget*  tabs_ = nullptr;
    QTableWidget* drawingsTable_  = nullptr;
    QTableWidget* templatesTable_ = nullptr;
};

} // namespace SketchCAD
