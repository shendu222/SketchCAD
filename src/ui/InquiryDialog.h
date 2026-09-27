#pragma once
// ============================================================================
// InquiryDialog —— 测量查询图形界面
// ----------------------------------------------------------------------------
// 把命令行测量命令（dist / area / massprop / id）封装为可视化面板：
//   * 几何计算全部复用 inquiry:: 核心层（与命令行同源、结果一致）；
//   * 只读、不改模型；菜单项与工具栏按钮共用 MainWindow::openInquiry() 打开本对话框。
// 设计原则：与工程内其它对话框（ModifyInputDialog / SectionDialog / TitleBlockForm）
// 保持一致的"模态 QDialog + 中文标签 + 图标 + 提示文字"风格。
// ============================================================================
#include <QDialog>
#include <QHBoxLayout>
#include "core/SketchModel.h"

class QTabWidget;
class QLineEdit;
class QPlainTextEdit;

namespace SketchCAD {

class InquiryDialog : public QDialog {
    Q_OBJECT
public:
    explicit InquiryDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    SketchModel* model_;
    QTabWidget* tabs_;
    QPlainTextEdit* result_;

    // ---- 距离页 ----
    QLineEdit *dX1_, *dY1_, *dX2_, *dY2_;
    // ---- 面积 / 质量特性页 ----
    QLineEdit* idList_;        // 图元 ID（空格分隔）；空 = 用当前选区
    QLineEdit* pointList_;     // 点列 "x1,y1 x2,y2 ..."
    // ---- 点坐标页 ----
    QLineEdit *iX_, *iY_;

    void doDist();
    void doArea(bool massprop);
    void doId();

    // X / Y 两列输入框的水平布局（供 QFormLayout 行内复用）
    static QHBoxLayout* row2(QLineEdit* a, QLineEdit* b);

    void appendResult(const QString& line);
    void clearResult();
};

} // namespace SketchCAD
