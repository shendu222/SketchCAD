#pragma once
#include <QString>
#include <QStringList>
#include <vector>
#include <map>

namespace SketchCAD {

// ===================== PAT 填充图案文件支持（AutoCAD .pat 格式）=====================
//
// 需求：剖面填充支持从「可执行文件所在目录下的 hatch 子目录」读取 .pat 图案文件，
// 并据此生成剖面线。.pat 为标准 AutoCAD 图案格式：
//
//   *NAME, 描述                      <- 图案头（* 开头；NAME 为图案名，逗号后为描述）
//   angle, x0, y0, dx, dy [, d1, d2, ...]   <- 一条图案线定义（可多行，构成整个图案）
//     angle : 线相对 X 轴的角度（度，逆时针）
//     x0,y0 : 图案锚点（基准点，单位与图面一致，本程序按 mm）
//     dx,dy : 相邻平行线在「图案局部坐标（未旋转）」中的偏移；最终偏移向量 =
//             R(angle) * (dx, dy)，其模长即为相邻线的实际间距（旋转保持距离不变）
//     d1,d2,... : 笔段序列，正负交替（正=落笔/实线段，负=抬笔/间隔），从第一个实线段开始；
//             沿每条线按该序列重复；长度 0 的段为填充占位，忽略
//
// 解析要点：一行 .pat 文件可含多个 *NAME 图案；每条定义行含若干笔段，行间用换行分隔，
// 行尾的 0 为占位（木纹类图案常见），解析时丢弃长度 <= 0 的笔段。

// 单条图案线定义
struct PatLine {
    double angleDeg = 0.0;
    double x0 = 0.0, y0 = 0.0;   // 基准点
    double dx = 0.0, dy = 0.0;   // 相邻线偏移（局部坐标）
    std::vector<double> dashes;   // 笔段（正=实线/落笔，负=间隔/抬笔），从实线段起
};

// 一个完整图案（对应一个 *NAME 头）
struct PatPattern {
    QString name;                 // 图案内部名（* 后逗号前）
    QString description;          // 描述（逗号后）
    std::vector<PatLine> lines;  // 图案的全部线定义
};

// PAT 图案库：懒加载 exe 同级 hatch/ 目录下所有 .pat，按图案名（内部名或文件名，
// 大小写不敏感）索引。单例，首次访问自动从 applicationDirPath()/hatch 加载。
class HatchPatternLibrary {
public:
    // 单例（首次访问从 exe/hatch/ 懒加载）
    static HatchPatternLibrary& instance();

    // 从指定目录加载全部 *.pat（清空旧有内容后重新加载）。测试可注入临时目录。
    void loadFromDir(const QString& dir);

    // 按图案名查找（内部名或文件名，大小写不敏感）。找不到返回 nullptr。
    const PatPattern* find(const QString& name) const;

    // 解析单个 .pat 文件文本，返回其中所有图案（一个文件可含多个 *NAME）。
    // 已暴露为 public 以便测试与二次开发直接复用解析逻辑。
    static std::vector<PatPattern> parsePatText(const QString& text);

    bool empty() const { return patterns_.empty(); }
    int size() const { return (int)patterns_.size(); }
    QStringList names() const;

    QString loadedDir() const { return loadedDir_; }

private:
    HatchPatternLibrary() = default;

    void ensureLoaded();

    std::vector<PatPattern> patterns_;
    std::map<QString, int> index_;   // 小写键 -> patterns_ 下标
    QString loadedDir_;
    bool loaded_ = false;
};

} // namespace SketchCAD
