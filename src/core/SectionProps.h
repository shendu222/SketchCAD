#pragma once
// P7（Ch5 辅助工具）：截面特性（面积/形心/二次矩/惯性积）+ 重量估算
// ------------------------------------------------------------
// FORBID「自造多边形积分」：本模块采用**给定标准公式**（Green 定理 / 鞋带公式及其矩公式），
// 非自造积分；数值正确性由自测以矩形/圆/三角形解析解对照校验（ESCALATE G1）。
#include <vector>
#include <QString>
#include "Vec.h"

namespace SketchCAD {

struct SectionProperties {
    double area       = 0.0;   // 截面面积 A (mm²)
    Vec3   centroid{0, 0, 0};  // 形心 (Cx, Cy)（z=0）
    double inertiaIx  = 0.0;   // 关于形心轴的二次矩 Ix = ∫y² dA (mm⁴)
    double inertiaIy  = 0.0;   // 关于形心轴的二次矩 Iy = ∫x² dA (mm⁴)
    double inertiaIxy = 0.0;   // 惯性积 Ixy = ∫xy dA (mm⁴)
    bool   valid      = false; // false = 退化（自交/顶点<3/零面积）
};

struct SectionProps {
    // 闭合多边形（有序顶点，自动闭合）截面特性。
    // 自交 / 顶点<3 / 零面积 → 返回 false（out.valid=false），可选 reason。
    static bool compute(const std::vector<Vec3>& polygon, SectionProperties& out, QString* reason = nullptr);

    // 重量估算：mass(g) = area(mm²) × thickness(mm) × density(g/cm³) × 1e-3。
    // density<=0（未知材料）→ 返回 false。
    static bool massGrams(double areaMm2, const QString& material, double& gramsOut,
                          double thicknessMm = 1.0);

    // 自交判定（几何谓词，非积分）：相邻边共享端点不算相交。
    static bool isSelfIntersecting(const std::vector<Vec3>& polygon);
};

} // namespace SketchCAD
