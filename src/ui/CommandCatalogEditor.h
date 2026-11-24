#pragma once

#include "core/CommandCatalogConfig.h"
#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;

namespace SketchCAD {

/// 命令目录 JSON 维护界面：命令级与参数级 CRUD + 结构校验 + 持久化。
class CommandCatalogEditor : public QDialog {
public:
    explicit CommandCatalogEditor(QWidget* parent = nullptr);

private:
    void buildUi();
    void loadDocument();
    void saveDocument();
    void refreshAll();

    void refreshCommandTable();
    void refreshSchemaTable();
    void refreshParameterTable();
    void updateButtons();

    void addCommand();
    void deleteCommand();
    void modifyCommand();
    void viewCommand();

    void addSchemaParameter();
    void deleteSchemaParameter();
    void modifySchemaParameter();
    void viewSchemaParameter();

    void addCommandParameter();
    void deleteCommandParameter();
    void modifyCommandParameter();
    void viewCommandParameter();

    int selectedCommandRow() const;
    int selectedSchemaRow() const;
    int selectedParameterRow() const;

    void showError(const QString& message);
    void showInfo(const QString& message);

    CatalogDocument doc_;
    QString path_;
    QString lastError_;

    QLabel* pathLabel_ = nullptr;
    QTableWidget* commandTable_ = nullptr;
    QTableWidget* schemaTable_ = nullptr;
    QTableWidget* parameterTable_ = nullptr;

    QPushButton* saveButton_ = nullptr;
    QPushButton* reloadButton_ = nullptr;
};

} // namespace SketchCAD
