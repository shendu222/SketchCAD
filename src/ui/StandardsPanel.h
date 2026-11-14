#pragma once
#include <QWidget>
#include <QVector>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

#include "core/standards/ComplianceChecker.h"
#include "core/SketchModel.h"
#include "ui/GraphicsView.h"

namespace SketchCAD {

// 工程制图合规检查面板：列出 ComplianceChecker 检出的不合规项，
// 双击可跳转到对应图元（setSelected + zoomToElement）。
// 面板只经 SketchModel / GraphicsView 的既有 API 操作，不持有模型所有权。
class StandardsPanel : public QWidget {
    Q_OBJECT
public:
    explicit StandardsPanel(SketchModel* model, QWidget* parent = nullptr);

    // 多文档切换重绑
    void setModel(SketchModel* model);
    // 注入视图指针，用于双击跳转定位（MainWindow 构造期调用）
    void setView(GraphicsView* view);

    // 由命令经 notifyComplianceReport 回调注入最新检查结果并刷新列表
    void setIssues(const QVector<ComplianceIssue>& issues);

    // 依据当前 issues_ 重建列表
    void refresh();

private slots:
    void onRunCheck();                  // 「运行检查」按钮：派发 stdcheck 命令
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    SketchModel*  model_  = nullptr;
    GraphicsView* view_   = nullptr;
    QVector<ComplianceIssue> issues_;
    QListWidget*  list_ = nullptr;
    QPushButton*  runBtn_ = nullptr;
    QLabel*       summaryLabel_ = nullptr;
};

} // namespace SketchCAD
