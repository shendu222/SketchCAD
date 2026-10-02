#include "PlotStyle.h"
#include <QStringList>
#include <QColor>
#include <QRegularExpression>
#include <QLocale>
#include <cmath>

namespace SketchCAD {

// ============================================================================
// 序列化（向后兼容：新增键灰度/抖动用 value().toBool/toDouble 带默认值读取，
// 旧档无此键时保持 overrideGray=false / dither=false，行为与旧版完全一致）
// ============================================================================
QJsonObject PlotStyleEntry::toJson() const {
    QJsonObject o;
    o["overrideColor"] = overrideColor;
    o["color"]        = color.name(QColor::HexArgb);
    o["overrideWidth"] = overrideWidth;
    o["widthMm"]      = widthMm;
    o["overrideGray"] = overrideGray;   // 新增
    o["gray"]         = gray;           // 新增
    o["dither"]       = dither;         // 新增
    return o;
}

PlotStyleEntry PlotStyleEntry::fromJson(const QJsonObject& o) {
    PlotStyleEntry e;
    e.overrideColor = o.value("overrideColor").toBool(false);
    e.color         = QColor(o.value("color").toString());
    e.overrideWidth = o.value("overrideWidth").toBool(false);
    e.widthMm       = o.value("widthMm").toDouble(0.25);
    e.overrideGray  = o.value("overrideGray").toBool(false);   // 缺 key → false（旧档）
    e.gray          = o.value("gray").toDouble(0.0);
    e.dither        = o.value("dither").toBool(false);
    return e;
}

// ============================================================================
// 纯函数：灰度化 + 线型名互转
// ============================================================================
QColor plotGrayscale(const QColor& c, double gray) {
    if (!c.isValid()) return c;
    double g = gray;
    if (g < 0.0) g = 0.0;
    if (g > 1.0) g = 1.0;
    // 灰度值 → 8bit 强度。gray=0 → 0(黑)，gray=1 → 255(白)。
    const int v = int(std::lround(g * 255.0));
    return QColor(v, v, v, c.alpha());
}

bool plotStyleFromName(const QString& name, LineStyle* out) {
    if (!out) return false;
    const QString n = name.trimmed().toUpper();
    if (n == QLatin1String("SOLID"))              { *out = LineStyle::Solid;       return true; }
    if (n == QLatin1String("DASHED"))             { *out = LineStyle::Dashed;      return true; }
    if (n == QLatin1String("DOTTED"))             { *out = LineStyle::Dotted;      return true; }
    if (n == QLatin1String("DASHDOT"))            { *out = LineStyle::DashDot;     return true; }
    if (n == QLatin1String("DASHDOTDOT"))         { *out = LineStyle::DashDotDot;  return true; }
    if (n == QLatin1String("WAVE"))               { *out = LineStyle::Wave;        return true; }
    if (n == QLatin1String("DOUBLEBREAK"))        { *out = LineStyle::DoubleBreak; return true; }
    if (n == QLatin1String("THICKDASHED"))        { *out = LineStyle::ThickDashed; return true; }
    if (n == QLatin1String("THICKDASHDOT"))       { *out = LineStyle::ThickDashDot;return true; }
    return false;
}

QString plotStyleName(LineStyle ls) {
    switch (ls) {
        case LineStyle::Solid:       return QStringLiteral("SOLID");
        case LineStyle::Dashed:      return QStringLiteral("DASHED");
        case LineStyle::Dotted:      return QStringLiteral("DOTTED");
        case LineStyle::DashDot:     return QStringLiteral("DASHDOT");
        case LineStyle::DashDotDot:  return QStringLiteral("DASHDOTDOT");
        case LineStyle::Wave:        return QStringLiteral("WAVE");
        case LineStyle::DoubleBreak: return QStringLiteral("DOUBLEBREAK");
        case LineStyle::ThickDashed: return QStringLiteral("THICKDASHED");
        case LineStyle::ThickDashDot:return QStringLiteral("THICKDASHDOT");
    }
    return QStringLiteral("SOLID");
}

// ============================================================================
// 解析：图层 > 颜色 > 线型
// ============================================================================
ResolvedPlotStyle PlotStyleTable::resolve(const Element& el) const {
    ResolvedPlotStyle r;
    r.color   = el.color;
    r.widthMm = el.effectiveLineWidth();   // 自动线宽 → 当前线组粗/细档；手动 → 覆盖值
    r.gray    = false;
    r.grayValue = 0.0;
    r.dither  = false;

    // 条目级：图层优先，其次颜色，最后线型。首个命中即返回（不再向下匹配）。
    const PlotStyleEntry* hit = nullptr;
    auto lit = byLayer_.find(el.layer);
    if (lit != byLayer_.end()) {
        hit = &lit.value();
    } else {
        auto cit = byColor_.find(plotColorKey(el.color));
        if (cit != byColor_.end()) {
            hit = &cit.value();
        } else {
            auto sit = byStyle_.find((int)el.lineStyle);
            if (sit != byStyle_.end()) hit = &sit.value();
        }
    }
    if (hit) {
        if (hit->overrideColor) r.color   = hit->color;
        if (hit->overrideWidth) r.widthMm = hit->widthMm;
        if (hit->overrideGray) {
            r.gray      = true;
            r.grayValue = hit->gray;
            r.dither    = hit->dither;
        }
    }

    // 全局灰度：条目未指定时兜底（黑白打印机一键出图）
    if (globalGray_ && !r.gray) {
        r.gray      = true;
        r.grayValue = globalGrayValue_;
        r.dither    = globalDither_;
    }

    // 灰度化：把最终颜色按 grayValue 转为灰度（灰度化只改颜色，不动线宽）
    if (r.gray) r.color = plotGrayscale(r.color, r.grayValue);

    return r;
}

// ============================================================================
// 序列化（整表）：新增 byStyle / 全局灰度三键，旧档缺省即保持「不灰度」行为
// ============================================================================
QJsonObject PlotStyleTable::toJson() const {
    QJsonObject o;
    QJsonObject layers, colors, styles;
    for (auto it = byLayer_.begin(); it != byLayer_.end(); ++it)
        layers[it.key()] = it->toJson();
    for (auto it = byColor_.begin(); it != byColor_.end(); ++it) {
        // 键以 16 进制字符串保存，避免 JSON 数字精度问题
        colors[QString::number(it.key(), 16)] = it->toJson();
    }
    for (auto it = byStyle_.begin(); it != byStyle_.end(); ++it)
        styles[QString::number(it.key())] = it->toJson();
    o["layers"] = layers;
    o["colors"] = colors;
    o["styles"] = styles;                 // 新增：按线型映射（旧档无此 key → 空）
    o["globalGray"]     = globalGray_;    // 新增
    o["globalGrayValue"] = globalGrayValue_;
    o["globalDither"]   = globalDither_;
    return o;
}

void PlotStyleTable::fromJson(const QJsonObject& o) {
    byLayer_.clear();
    byColor_.clear();
    byStyle_.clear();
    const QJsonObject layers = o.value("layers").toObject();
    for (auto it = layers.begin(); it != layers.end(); ++it)
        byLayer_[it.key()] = PlotStyleEntry::fromJson(it->toObject());
    const QJsonObject colors = o.value("colors").toObject();
    for (auto it = colors.begin(); it != colors.end(); ++it)
        byColor_[it.key().toInt(0, 16)] = PlotStyleEntry::fromJson(it->toObject());
    const QJsonObject styles = o.value("styles").toObject();   // 缺 key → 空（向后兼容）
    for (auto it = styles.begin(); it != styles.end(); ++it)
        byStyle_[it.key().toInt()] = PlotStyleEntry::fromJson(it->toObject());
    globalGray_      = o.value("globalGray").toBool(false);
    globalGrayValue_ = o.value("globalGrayValue").toDouble(0.0);
    globalDither_    = o.value("globalDither").toBool(false);
}

PlotStyleTable PlotStyleTable::defaultTable() { return PlotStyleTable(); }

// ============================================================================
// GB/T 14665-2012 黑白预设
// ----------------------------------------------------------------------------
// 核心思路：彩色图靠颜色区分粗/细，黑白出图必须靠「线宽」区分。
// 因此预设按线型给出标准线宽：粗线型（粗实/粗虚/粗点画）取 groupThickMm，
// 细线型取 groupThinMm；全部灰度=0（纯黑）且不抖动（线宽图轮廓要锐利）。
// ============================================================================
PlotStyleTable PlotStyleTable::gbPreset(LineWidthGroup group) {
    PlotStyleTable t;
    const double thick = groupThickMm(group);
    const double thin  = groupThinMm(group);
    auto mk = [](double w) {
        PlotStyleEntry e;
        e.overrideWidth = true;
        e.widthMm       = w;
        e.overrideGray  = true;
        e.gray          = 0.0;   // 纯黑
        e.dither        = false; // 线宽图不抖动
        return e;
    };
    // 粗档：粗实线 / 粗虚线 / 粗点画线
    t.setStyleStyle(LineStyle::Solid,       mk(thick));
    t.setStyleStyle(LineStyle::ThickDashed, mk(thick));
    t.setStyleStyle(LineStyle::ThickDashDot,mk(thick));
    // 细档：细实线 / 虚线 / 点线 / 点划线 / 双点划线 / 断裂线
    t.setStyleStyle(LineStyle::Dashed,      mk(thin));
    t.setStyleStyle(LineStyle::Dotted,      mk(thin));
    t.setStyleStyle(LineStyle::DashDot,     mk(thin));
    t.setStyleStyle(LineStyle::DashDotDot,  mk(thin));
    t.setStyleStyle(LineStyle::Wave,        mk(thin));
    t.setStyleStyle(LineStyle::DoubleBreak, mk(thin));
    return t;
}

PlotStyleTable PlotStyleTable::gbPresetGrayDither() {
    PlotStyleTable t = gbPreset(currentLineWidthGroup());
    // 全局灰度 + 抖动：适合剖面/填充等中间调层次表达（细线仍纯黑、宽面用抖动灰）
    t.setGlobalGray(true, 0.0, /*dither=*/true);
    return t;
}

// ============================================================================
// 简化 CTB 文本导入 / 导出
// ============================================================================
// 数值解析（locale 无关）：QString::toDouble 依赖系统小数点设置（de_DE 下 '.' 解析失败），
// 且 QLocale::c().toDouble 在部分 Qt/MinGW 组合下对合法串也回报失败。
// 稳妥做法：统一把 ',' 归一为 '.'，再走 QString::toDouble（配合 C 默认小数点）。
static bool ctbToDouble(const QString& s, double* out) {
    QString t = s.trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (ok && out) *out = v;
    return ok;
}

bool PlotStyleTable::fromCtb(const QString& text, PlotStyleTable* out, QString* err) {
    if (!out) { if (err) *err = QStringLiteral("输出指针为空"); return false; }
    out->clearAll();
    out->setGlobalGray(false, 0.0, false);
    bool ok = true;
    QString firstErr;

    auto setErr = [&](const QString& m) { if (firstErr.isEmpty()) firstErr = m; ok = false; };

    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                         Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        QString ln = raw.trimmed();
        if (ln.isEmpty() || ln.startsWith(QLatin1Char('#'))) continue;
        // 去掉行尾注释。注意：不能简单按 '#' 截断——COLOR #FF0000 这类十六进制颜色
        // 也含 '#'。规则：'#' 后紧跟 hex 数字串(#RGB/#RRGGBB/#AARRGGBB)视为颜色而非注释。
        {
            int cut = -1;
            for (int i = 0; i < ln.size(); ++i) {
                if (ln[i] != QLatin1Char('#')) continue;
                // 统计其后连续 hex 字符长度
                int j = i + 1, hex = 0;
                auto isHex = [](QChar c){ return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'); };
                while (j < ln.size() && isHex(ln[j])) { ++j; ++hex; }
                // 长度 3/6/8 且其后不是 hex → 颜色 token，不是注释
                if ((hex == 3 || hex == 6 || hex == 8)) continue;
                cut = i;   // 否则视为注释起点
                break;
            }
            if (cut >= 0) ln = ln.left(cut).trimmed();
        }
        if (ln.isEmpty()) continue;

        const QStringList tok = ln.split(QRegularExpression(QStringLiteral("\\s+")),
                                         Qt::SkipEmptyParts);
        if (tok.isEmpty()) continue;
        const QString kw = tok[0].toUpper();

        // ---- GRAYSCALE <v> [DITHER=0|1] ----
        if (kw == QLatin1String("GRAYSCALE")) {
            if (tok.size() < 2) { setErr(QStringLiteral("GRAYSCALE 缺灰度值: ") + ln); continue; }
            double g = 0.0;
            if (!ctbToDouble(tok[1], &g)) { setErr(QStringLiteral("GRAYSCALE 灰度值非法: ") + tok[1]); continue; }
            bool dith = false;
            for (int i = 2; i < tok.size(); ++i) {
                if (tok[i].startsWith(QLatin1String("DITHER="), Qt::CaseInsensitive))
                    dith = (tok[i].mid(7).trimmed() != QLatin1String("0"));
            }
            out->setGlobalGray(true, g, dith);
            continue;
        }

        // ---- LAYER / COLOR / STYLE <target> [KEY=VAL ...] ----
        if (kw != QLatin1String("LAYER") && kw != QLatin1String("COLOR") &&
            kw != QLatin1String("STYLE")) {
            setErr(QStringLiteral("未知关键字: ") + tok[0]);
            continue;
        }
        if (tok.size() < 2) { setErr(kw + QStringLiteral(" 缺目标: ") + ln); continue; }

        PlotStyleEntry e;
        // COLOR 的目标色即匹配键（#rrggbb 或 #aarrggbb）；LAYER/STYLE 的目标分别是名字/线型名
        QString target = tok[1];
        bool lineBad = false;   // 本行是否有解析错误 → 有则整行丢弃（不写半成品条目）

        for (int i = 2; i < tok.size(); ++i) {
            const QString& t = tok[i];
            const int eq = t.indexOf(QLatin1Char('='));
            if (eq <= 0) { setErr(QStringLiteral("参数应为 KEY=VAL: ") + t); lineBad = true; continue; }
            const QString k = t.left(eq).toUpper();
            const QString v = t.mid(eq + 1);
            if (k == QLatin1String("WIDTH")) {
                double w = 0.0;
                if (!ctbToDouble(v, &w) || w <= 0) { setErr(QStringLiteral("WIDTH 非法: ") + v); lineBad = true; continue; }
                e.overrideWidth = true; e.widthMm = w;
            } else if (k == QLatin1String("GRAY")) {
                double g = 0.0;
                if (!ctbToDouble(v, &g) || g < 0.0 || g > 1.0) { setErr(QStringLiteral("GRAY 非法(需0..1): ") + v); lineBad = true; continue; }
                e.overrideGray = true; e.gray = g;
            } else if (k == QLatin1String("DITHER")) {
                e.dither = (v.trimmed() != QLatin1String("0"));
            } else if (k == QLatin1String("COLOR")) {
                QColor c(v);
                if (!c.isValid()) { setErr(QStringLiteral("COLOR 非法: ") + v); lineBad = true; continue; }
                e.overrideColor = true; e.color = c;
            } else {
                setErr(QStringLiteral("未知参数: ") + t);
                lineBad = true;
            }
        }
        if (lineBad) continue;   // 整行丢弃，不写半成品

        if (kw == QLatin1String("LAYER")) {
            out->setLayerStyle(target, e);
        } else if (kw == QLatin1String("COLOR")) {
            QColor key(target);
            if (!key.isValid()) { setErr(QStringLiteral("COLOR 匹配键非法: ") + target); continue; }
            out->setColorStyle(key, e);
        } else { // STYLE
            LineStyle ls;
            if (!plotStyleFromName(target, &ls)) { setErr(QStringLiteral("未知线型: ") + target); continue; }
            out->setStyleStyle(ls, e);
        }
    }
    if (err) *err = firstErr;
    return ok;
}

QString PlotStyleTable::toCtb() const {
    QStringList L;
    L << QStringLiteral("# SketchCAD 简化 CTB 打印样式表");
    L << QStringLiteral("# 语法: LAYER/COLOR/STYLE <目标> [WIDTH=mm] [GRAY=0..1] [DITHER=0|1] [COLOR=#rrggbb]");
    if (globalGray_)
        L << QStringLiteral("GRAYSCALE %1 DITHER=%2")
                 .arg(globalGrayValue_, 0, 'f', 3).arg(globalDither_ ? 1 : 0);
    auto entryTail = [](const PlotStyleEntry& e) {
        QStringList t;
        if (e.overrideWidth) t << QStringLiteral("WIDTH=%1").arg(e.widthMm, 0, 'f', 3);
        if (e.overrideGray)  t << QStringLiteral("GRAY=%1").arg(e.gray, 0, 'f', 3);
        if (e.dither)        t << QStringLiteral("DITHER=1");
        if (e.overrideColor) t << QStringLiteral("COLOR=%1").arg(e.color.name());
        return t.join(QLatin1Char(' '));
    };
    for (auto it = byLayer_.begin(); it != byLayer_.end(); ++it)
        L << QStringLiteral("LAYER %1 %2").arg(it.key(), entryTail(it.value()));
    for (auto it = byColor_.begin(); it != byColor_.end(); ++it)
        L << QStringLiteral("COLOR #%1 %2")
                 .arg(QString::number(it.key() & 0x00FFFFFF, 16).rightJustified(6, QLatin1Char('0')),
                      entryTail(it.value()));
    for (auto it = byStyle_.begin(); it != byStyle_.end(); ++it)
        L << QStringLiteral("STYLE %1 %2")
                 .arg(plotStyleName((LineStyle)it.key()), entryTail(it.value()));
    return L.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace SketchCAD
