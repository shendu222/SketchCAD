#include "TextStyle.h"
#include "SketchModel.h"
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QFontMetricsF>

namespace SketchCAD {

QString resolveFields(const QString& raw, const SketchModel* model) {
    if (!raw.contains("%<")) return raw;   // 快速路径：无占位直接返回
    QString out = raw;
    auto repl = [&](const QString& code, const QString& val) {
        out.replace(QStringLiteral("%<") + code + QStringLiteral(">%"), val);
    };
    const QDateTime now = QDateTime::currentDateTime();
    repl(QStringLiteral("DATE"), now.toString(QStringLiteral("yyyy-MM-dd")));
    repl(QStringLiteral("TIME"), now.toString(QStringLiteral("HH:mm:ss")));
    if (model) {
        const QString path = model->drawingPath();
        repl(QStringLiteral("FILENAME"), path.isEmpty() ? QStringLiteral("未命名") : QFileInfo(path).fileName());
        repl(QStringLiteral("FILEPATH"), path.isEmpty() ? QStringLiteral("未命名") : path);
        repl(QStringLiteral("COUNT"),    QString::number(model->elementCount()));
    } else {
        repl(QStringLiteral("FILENAME"), QStringLiteral("未命名"));
        repl(QStringLiteral("FILEPATH"), QStringLiteral("未命名"));
        repl(QStringLiteral("COUNT"),    QStringLiteral("0"));
    }
    return out;
}

ResolvedTextStyle resolveTextStyle(const Element& e, const SketchModel* model) {
    ResolvedTextStyle r;
    const TextStyle* ts = nullptr;
    if (model && !e.styleName.isEmpty()) ts = model->findTextStyle(e.styleName);
    r.fontFamily     = ts ? ts->fontFamily : QString();
    r.widthFactor    = (ts ? ts->widthFactor : 1.0) * (e.widthFactor > 0 ? e.widthFactor : 1.0);
    r.obliquingAngle = ts ? ts->obliquingAngle : 0.0;
    r.bold           = ts ? ts->bold   : false;
    r.italic         = ts ? ts->italic : false;
    // 字高：样式有非零字高则优先（AutoCAD 语义），否则用图元自身 radius
    const double styleH = ts ? ts->textHeight : 0.0;
    r.height = (styleH > 0.1) ? styleH : (e.radius > 0.1 ? e.radius : 3.5);
    return r;
}

QStringList wrapText(const QString& text, double maxWidthMm, const QFont& font) {
    QStringList result;
    if (maxWidthMm <= 0.0) { result = text.split('\n'); return result; }
    const QFontMetricsF fm(font);
    const QStringList paras = text.split('\n');
    for (const QString& para : paras) {
        if (para.isEmpty()) { result.append(QString()); continue; }
        QString line;
        double width = 0.0;
        for (const QChar& ch : para) {
            const double cw = fm.horizontalAdvance(QString(ch));
            if (!line.isEmpty() && width + cw > maxWidthMm) {
                result.append(line);
                line.clear(); width = 0.0;
            }
            line += ch;
            width += cw;
        }
        if (!line.isEmpty()) result.append(line);
    }
    return result;
}

} // namespace SketchCAD
