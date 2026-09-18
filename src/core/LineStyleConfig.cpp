#include "LineStyleConfig.h"
#include "LineWidthGroup.h"

namespace SketchCAD {

LineStyleConfig& LineStyleConfig::instance() {
    static LineStyleConfig inst;          // 函数局部静态：线程安全、惰性初始化
    return inst;
}

LineStyleConfig::LineStyleConfig() {
    // 默认不做显式覆盖：线宽 = 当前线组的粗/细档（见 defaultWidth）。
    // 需要临时固定某线型宽度时，运行期调用 setDefaultWidth() 即可。
}

// GB/T 14665-2012 表1：粗线档 vs 细线档归类
bool lineStyleIsThick(LineStyle ls) {
    switch (ls) {
        case LineStyle::Solid:
        case LineStyle::ThickDashed:
        case LineStyle::ThickDashDot:
            return true;
        default:
            return false;   // 细虚线/点线/点划线/双点划线/波浪线/双折线
    }
}

double LineStyleConfig::defaultWidth(LineStyle ls) const {
    auto it = map_.find(ls);
    if (it != map_.end()) return it.value();    // 显式覆盖优先
    return lineStyleIsThick(ls) ? groupThickMm(currentLineWidthGroup())
                                : groupThinMm(currentLineWidthGroup());
}

void LineStyleConfig::setDefaultWidth(LineStyle ls, double mm) {
    if (mm > 0) map_[ls] = mm;                  // 仅接受正宽度
}

void LineStyleConfig::clearOverride(LineStyle ls) {
    map_.remove(ls);
}

double defaultWidthForStyle(LineStyle ls) {
    return LineStyleConfig::instance().defaultWidth(ls);
}

} // namespace SketchCAD
