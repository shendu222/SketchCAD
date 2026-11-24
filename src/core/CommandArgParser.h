#pragma once

#include <QStringList>
#include <vector>

namespace SketchCAD {
namespace arg {

struct RectArrayArgs {
    int rows = 0;
    int cols = 0;
    double rowSpacing = 0.0;
    double colSpacing = 0.0;
};

// arrayrect 的参数解析：把 4 个 toInt/toDouble + ok 校验收敛到一个纯函数。
// 纯函数不写日志，错误文案仍由调用点给出，保持命令语义与本地化策略不变。
inline bool parseRectArray(const QStringList& toks, RectArrayArgs& out) {
    if (toks.size() < 5) return false;
    bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
    const int rows = toks[1].toInt(&ok1);
    const int cols = toks[2].toInt(&ok2);
    const double rowSpacing = toks[3].toDouble(&ok3);
    const double colSpacing = toks[4].toDouble(&ok4);
    if (!ok1 || !ok2 || !ok3 || !ok4 || rows < 1 || cols < 1) return false;
    out.rows = rows;
    out.cols = cols;
    out.rowSpacing = rowSpacing;
    out.colSpacing = colSpacing;
    return true;
}

// 从 toks[from..] 解析 id 列表；任一 token 非法即整体失败。
// list/area/massprop 等查询命令共用，避免逐条复制 toInt(&ok) 循环。
inline bool parseIds(const QStringList& toks, int from, std::vector<int>& ids,
                     int* badIndex = nullptr) {
    ids.clear();
    for (int i = from; i < toks.size(); ++i) {
        bool ok = false;
        const int id = toks[i].toInt(&ok);
        if (!ok) {
            if (badIndex) *badIndex = i;
            return false;
        }
        ids.push_back(id);
    }
    return !ids.empty();
}

} // namespace arg
} // namespace SketchCAD
