#include "TitleBlockForm.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/SheetSpec.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScrollArea>

namespace SketchCAD {

TitleBlockForm::TitleBlockForm(SketchModel* model, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("填写标题栏"));
    resize(420, 460);

    // 收集当前图纸中的标题栏栏目（name 以 "tb:field:" 开头）
    struct Row { int id; QString key; QString label; QString value; QLineEdit* edit; };
    QVector<Row> rows;

    const auto& fields = titleBlockFields();
    for (const auto& f : fields) {
        const QString name = QStringLiteral("tb:field:") + f.key;
        for (const auto& e : model->elements()) {
            if (e.name == name) {
                Row r;
                r.id = e.id;
                r.key = f.key;
                r.label = f.label;
                r.value = e.text;
                rows.append(r);
                break;
            }
        }
    }

    auto* root = new QVBoxLayout(this);
    if (rows.isEmpty()) {
        root->addWidget(new QLabel(QStringLiteral("当前图纸未检测到标准标题栏。\n请先通过「文件 - 新建」生成含标题栏的图纸。"), this));
    } else {
        auto* form = new QFormLayout();
        for (auto& r : rows) {
            auto* le = new QLineEdit(r.value, this);
            le->setPlaceholderText(QStringLiteral("请输入%1").arg(r.label));
            r.edit = le;
            form->addRow(r.label, le);
        }
        auto* sa = new QScrollArea(this);
        auto* wrap = new QWidget(this);
        wrap->setLayout(form);
        sa->setWidget(wrap);
        sa->setWidgetResizable(true);
        root->addWidget(sa);
    }

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                    Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this, [this, model, rows]() mutable {
        for (const auto& r : rows) {
            const QString v = r.edit->text();
            if (v != r.value) {
                model->updateElement(r.id, [v](Element& e) { e.text = v; });
            }
        }
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
