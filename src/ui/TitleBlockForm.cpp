#include "TitleBlockForm.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/SheetSpec.h"
#include "core/TitleBlockLayout.h"   // titleBlockCellAt / fitTitleBlockText：编辑期单元格适配
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

    // 收集当前图纸中的标题栏栏目（name 以 "tb:field:" 开头）。
    // 同名元素全收集（不 break）：代号栏（图框左上角）字段与标题栏「图号」共用
    // tb:field:drawingNumber，确认后同名全部写回 → 填图号即代号栏自动关联同步。
    // 代号栏元素锚点在图框周边带、不在标题栏网格内，titleBlockCellAt 返回 invalid，
    // fitTitleBlockText 对 invalid 不动（绝不猜），故写回对其天然安全。
    struct Row { QString key; QString label; QString value; QVector<int> ids;
                 QVector<Vec3> anchors; QLineEdit* edit; };
    QVector<Row> rows;

    const auto& fields = titleBlockFields();
    for (const auto& f : fields) {
        const QString name = QStringLiteral("tb:field:") + f.key;
        Row r;
        r.key = f.key;
        r.label = f.label;
        for (const auto& e : model->elements()) {
            if (e.name == name) {
                if (r.ids.isEmpty()) r.value = e.text;   // 首个匹配作表单显示值
                r.ids.append(e.id);
                r.anchors.append(e.pts.empty() ? Vec3() : e.pts[0]);
            }
        }
        if (!r.ids.isEmpty()) rows.append(r);
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
                // 逐元素写回：标题栏栏目做编辑期单元格适配（按开表单时的锚点重建
                // 所在单元格；网格线在模态对话框期间不变，只读预计算避开
                // updateElement 期间的模型变动），写回时同步收缩字高/复位锚点；
                // 代号栏等非标题栏网格元素 cell 无效 → fitTitleBlockText 不动。
                for (int i = 0; i < r.ids.size(); ++i) {
                    const int id = r.ids[i];
                    const Vec3& anchor = r.anchors[i];
                    const QRectF cell = titleBlockCellAt(model, anchor.x, anchor.y);
                    model->updateElement(id, [v, cell](Element& e) {
                        e.text = v;
                        fitTitleBlockText(e, cell);
                    });
                }
            }
        }
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
