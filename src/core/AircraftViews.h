#pragma once
#include <vector>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

class SketchModel;

namespace aircraft {

// 飞机三面图（正面图 / 侧面图 / 俯视图）参数模板。
// 所有尺寸默认取自用户提供的附图，单位：米。
struct AircraftViewsParams {
    double length          = 38.90;   // 机身总长
    double wingspan        = 35.80;   // 翼展
    double fuselageHeight  =  6.13;   // 机身高度
    double tailHeight      =  5.99;   // 垂尾高度
    double engineX         = 12.08;   // 发动机距机头
    double engineY         = 12.60;   // 发动机距机身中心线
    double noseGearX       =  5.28;   // 前起落架距机头
    double mainGearX       = 18.75;   // 主起落架距机头（= 5.28 + 13.47）
    double mainGearTrack   =  7.62;   // 主起落架轮距
    double outerWidth      =  8.98;   // 前轮/底部外廓宽度
};

// 根据参数生成三面图所需的全部图元（直线、圆、圆弧、椭圆、标注、文字）。
// 返回的图元尚未写入模型，调用方应自行 beginEdit() + addElement()。
std::vector<Element> buildThreeViews(const AircraftViewsParams& p,
                                     const SketchModel* model);

} // namespace aircraft
} // namespace SketchCAD
