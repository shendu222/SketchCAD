#pragma once
#include <QString>

namespace SketchCAD {

// 尺寸标注遵循的标准。默认值必须放在枚举首位，以便旧 .sketch 文件未存该字段时回退为 GB/T。
// 枚举值顺序只增不改：已存盘 .sketch 文件里存的是整数。
enum class DimensionStandard {
    GBT = 0,   // GB/T 4458.4 / GB/T 4458.5 系列（中国机械制图国家标准）
    ISO = 1    // ISO 129-1 / ISO 14405 系列（国际技术产品文档标准）
};

inline const char* dimensionStandardName(DimensionStandard s) {
    switch (s) {
        case DimensionStandard::GBT: return "GB/T 4458";
        case DimensionStandard::ISO: return "ISO 129";
    }
    return "GB/T 4458";
}

// 尺寸线终端（箭头）形式。GB/T 默认实心闭合；ISO 默认闭合（实心或空心依企业约定），
// 但 ISO 129-1:2018 图 12 也允许斜线、圆点等形式，故提供可扩展枚举。
enum class ArrowForm {
    ClosedFilled = 0,   // 实心闭合三角箭头（GB/T 4458.4 默认）
    ClosedUnfilled,     // 空心闭合三角箭头
    Open30,             // 开口 30° 箭头（ISO 建筑/通用图常见，机械图不推荐但可切换）
    Open90,             // 开口 90° 箭头
    ObliqueStroke,      // 斜线终端（GB/T 允许的小空间替代形式；ISO 亦允许）
    Dot                 // 圆点终端（小尺寸替代）
};

inline const char* arrowFormName(ArrowForm f) {
    switch (f) {
        case ArrowForm::ClosedFilled: return "实心闭合";
        case ArrowForm::ClosedUnfilled: return "空心闭合";
        case ArrowForm::Open30: return "开口 30°";
        case ArrowForm::Open90: return "开口 90°";
        case ArrowForm::ObliqueStroke: return "斜线";
        case ArrowForm::Dot: return "圆点";
    }
    return "实心闭合";
}

// 文字方向策略。
// GB/T 4458.4-2003 方法 1：数字按图 18 方向注写（可读方向规则，避免 30° 范围内倒置）。
// GB/T 方法 2 / ISO 129-1：文字可与尺寸线对齐，或一律水平。
enum class TextOrientation {
    AlignedReadable = 0,   // 与尺寸线对齐，但保证永远不倒置（GB/T 方法 1 默认）
    Horizontal,          // 一律水平（GB/T 方法 2；小尺寸/坐标标注常用）
    AlignedISO           // 严格平行于尺寸线，可读方向按 ISO（从下/从右读）
};

inline const char* textOrientationName(TextOrientation o) {
    switch (o) {
        case TextOrientation::AlignedReadable: return "对齐可读";
        case TextOrientation::Horizontal: return "水平";
        case TextOrientation::AlignedISO: return "ISO 对齐";
    }
    return "对齐可读";
}

// 公差注写形式。两种标准都支持极限偏差与极限尺寸，但默认表达习惯不同。
enum class ToleranceStyle {
    None = 0,          // 不注公差
    Symmetric,         // 50 ±0.1
    DeviationStacked,  // 50 上偏差/下偏差（GB/T 常用堆叠；ISO 亦允许）
    LimitSize,         // 50 max. / 50 min. / 50 max.2（ISO 常用）
    FitCode            // 50 H7 / 50 g6（配合代号，放在基本尺寸之后）
};

inline const char* toleranceStyleName(ToleranceStyle t) {
    switch (t) {
        case ToleranceStyle::None: return "无";
        case ToleranceStyle::Symmetric: return "对称 ±";
        case ToleranceStyle::DeviationStacked: return "极限偏差";
        case ToleranceStyle::LimitSize: return "极限尺寸";
        case ToleranceStyle::FitCode: return "配合代号";
    }
    return "无";
}

// 每个标准下的一套默认配置。不含数值大小，只含「形式」选择；
// 具体箭头长/字高等尺寸由 dim::Style 持有。
struct DimensionStandardConfig {
    DimensionStandard standard = DimensionStandard::GBT;
    ArrowForm arrowForm = ArrowForm::ClosedFilled;
    TextOrientation textOrientation = TextOrientation::AlignedReadable;
    ToleranceStyle toleranceStyle = ToleranceStyle::None;

    // GB/T 4458.4 默认：实心闭合箭头、方法 1 对齐可读文字、无公差
    static DimensionStandardConfig gbtDefault() {
        return {DimensionStandard::GBT, ArrowForm::ClosedFilled,
                TextOrientation::AlignedReadable, ToleranceStyle::None};
    }

    // ISO 129-1 默认：闭合空心箭头、文字与尺寸线对齐、极限偏差
    static DimensionStandardConfig isoDefault() {
        return {DimensionStandard::ISO, ArrowForm::ClosedUnfilled,
                TextOrientation::AlignedISO, ToleranceStyle::DeviationStacked};
    }

    static DimensionStandardConfig forStandard(DimensionStandard s) {
        return (s == DimensionStandard::ISO) ? isoDefault() : gbtDefault();
    }
};

} // namespace SketchCAD
