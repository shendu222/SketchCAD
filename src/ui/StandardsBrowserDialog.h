#pragma once
#include <QDialog>
#include <QString>

class QListWidget;
class QTableWidget;
class QLabel;
class QLineEdit;

namespace SketchCAD {

// 标准知识库浏览面板（只读）：左侧列出 12 类国标表，右侧表格展示所选表的全部键值。
// 与 standards 命令、query_standards LLM 工具共用 StandardsDB::instance() 单例数据源，
// 数据内嵌于可执行文件（:/standards/<name>.json），部署零附件。
class StandardsBrowserDialog : public QDialog {
    Q_OBJECT
public:
    explicit StandardsBrowserDialog(QWidget* parent = nullptr);

private slots:
    void onTableSelected();
    void onFilterChanged(const QString& text);

private:
    void populateTable(const QString& table);

    QListWidget* list_   = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel*      srcLabel_ = nullptr;
    QLineEdit*   filter_ = nullptr;
};

} // namespace SketchCAD
