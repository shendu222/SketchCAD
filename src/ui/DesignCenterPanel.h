#pragma once
#include <QWidget>
#include <QString>
#include <QHash>
#include <vector>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QComboBox;

namespace SketchCAD {

class SketchModel;
struct BlockDef;

// 设计中心（符号库浏览 / 插入）—— 侧栏树分两组列出块定义：
//   * 「当前图块表」：本图 SketchModel 块表中的全部块（随 blocksChanged 实时刷新）。
//   * 「外部文件块表」：从外部 SketchCAD 存档 JSON / DXF 的 BLOCKS 段读取的块定义。
// 双击任一块 → 在当前视图中心插入一个块引用（外部块先自动并入本图块表再插入）。
class DesignCenterPanel : public QWidget {
    Q_OBJECT
public:
    explicit DesignCenterPanel(SketchModel* model, QWidget* parent = nullptr);

    // 供主窗口在切换图纸 / 块表变化时调用
    void refresh();

signals:
    // 请求在当前视图中心插入指定块（由 MainWindow 连接实际的 GraphicsView 求中心点）
    void insertBlockRequested(const QString& blockName);

private slots:
    void onLoadExternal();
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    // 外部块：文件名 → 该文件的块定义列表（双击时按名查回并并入本图）
    void addExternalFile(const QString& path);

    SketchModel*   model_;
    QTreeWidget*   tree_    = nullptr;
    QLabel*        info_    = nullptr;
    QPushButton*   loadBtn_ = nullptr;

    // 外部文件块表缓存：绝对路径 → (显示名, 块定义列表)
    struct ExternalSet { QString label; std::vector<BlockDef> defs; };
    std::vector<ExternalSet> externals_;
    // 双击的外部块来源：块名 → 所属外部集合索引
    QHash<QString, int> externalIndex_;
};

} // namespace SketchCAD
