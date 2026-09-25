#include "HatchLibrary.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <cmath>

namespace SketchCAD {

// 解析单个 .pat 文件文本：按 '*' 头切分多个图案；每个图案含若干定义行。
// 笔段序列中长度 <= 1e-6 的段视为填充占位，丢弃（木纹类图案行尾常见 0）。
std::vector<PatPattern> HatchPatternLibrary::parsePatText(const QString& text) {
    std::vector<PatPattern> out;
    PatPattern* cur = nullptr;
    const QStringList lines = text.split('\n');
    for (const QString& raw : lines) {
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(';') || line.startsWith('!')) continue;   // 注释行
        if (line.startsWith('*')) {                                   // 图案头
            QString body = line.mid(1).trimmed();
            PatPattern p;
            int comma = body.indexOf(',');
            if (comma >= 0) {
                p.name = body.left(comma).trimmed();
                p.description = body.mid(comma + 1).trimmed();
            } else {
                p.name = body;
            }
            out.push_back(std::move(p));
            cur = &out.back();
            continue;
        }
        // 定义行：angle, x0, y0, dx, dy [, 笔段...]
        const QStringList parts = line.split(',', Qt::SkipEmptyParts);
        QList<double> nums;
        nums.reserve(parts.size());
        for (const QString& s : parts) {
            bool ok = false;
            double v = s.trimmed().toDouble(&ok);
            if (ok) nums.append(v);
        }
        if (nums.size() < 5) continue;                                // 非法定义行，跳过
        PatLine pl;
        pl.angleDeg = nums[0];
        pl.x0 = nums[1];
        pl.y0 = nums[2];
        pl.dx = nums[3];
        pl.dy = nums[4];
        for (int i = 5; i < nums.size(); ++i)
            if (std::fabs(nums[i]) > 1e-6) pl.dashes.push_back(nums[i]);
        if (!cur) {                                                   // 头缺失：匿名收容
            PatPattern p; p.name = QStringLiteral("PATTERN");
            out.push_back(std::move(p)); cur = &out.back();
        }
        cur->lines.push_back(std::move(pl));
    }
    return out;
}

void HatchPatternLibrary::loadFromDir(const QString& dir) {
    loadedDir_ = dir;
    patterns_.clear();
    index_.clear();
    QDir d(dir);
    if (!d.exists()) { loaded_ = true; return; }
    const QStringList files = d.entryList(QStringList() << "*.pat" << "*.PAT", QDir::Files);
    for (const QString& f : files) {
        QFile file(d.filePath(f));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream ts(&file);
        ts.setAutoDetectUnicode(true);
        const QString text = ts.readAll();
        file.close();
        const std::vector<PatPattern> ps = parsePatText(text);
        const QString baseKey = f.section('.', 0, 0).toLower();        // 文件名（去扩展名）键
        bool firstOfFile = true;
        for (const PatPattern& p : ps) {
            patterns_.push_back(p);
            const int idx = (int)patterns_.size() - 1;
            const QString k = p.name.toLower();
            if (!k.isEmpty() && index_.find(k) == index_.end()) index_[k] = idx;
            if (firstOfFile) {
                if (index_.find(baseKey) == index_.end()) index_[baseKey] = idx;
                firstOfFile = false;
            }
        }
    }
    loaded_ = true;
}

const PatPattern* HatchPatternLibrary::find(const QString& name) const {
    auto it = index_.find(name.toLower());
    if (it == index_.end()) return nullptr;
    return &patterns_[it->second];
}

QStringList HatchPatternLibrary::names() const {
    QStringList r;
    for (const auto& p : patterns_) r.append(p.name);
    return r;
}

void HatchPatternLibrary::ensureLoaded() {
    if (loaded_) return;
    QString appDir;
    if (QCoreApplication::instance())
        appDir = QCoreApplication::applicationDirPath();
    loadFromDir(appDir + QStringLiteral("/hatch"));
    loaded_ = true;
}

HatchPatternLibrary& HatchPatternLibrary::instance() {
    static HatchPatternLibrary lib;
    lib.ensureLoaded();
    return lib;
}

} // namespace SketchCAD
