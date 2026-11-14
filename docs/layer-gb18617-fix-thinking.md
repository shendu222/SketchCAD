# GB/T 18617 / ISO 13567 图层命名功能 —— 修复深思录

> 本文记录 `src/core/Layer.cpp` 在接入 GB/T 18617 / ISO 13567 图层命名建议功能时，
> 所遭遇的**链接期 `undefined reference` 错误**的完整排障思路。
> 它与《`layer-gb18617-design-analysis.md`（实现设计）》互补：
> 设计文档讲"怎么造"，本篇讲"造的时候为什么塌了、怎么把它扶起来"。

---

## 0. 背景：这次"修复"指的到底是什么

GB/T 18617 / ISO 13567 图层命名建议与合规检查是一次新增功能（非修旧 bug），
但它在落地过程中**自己产生了一个编译器发现不了、链接器才暴露的 bug**，
本文把对这个 bug 的排查称为"本次修复"。

- 实现设计文档已交付：`docs/layer-gb18617-design-analysis.md`
- 本文聚焦排障：**Layer.cpp 命名空间不匹配 → 链接期 `undefined reference`**。

---

## 1. 症状（Symptom）

`SketchCAD` 与 `SketchSelfTest` 两个 target 在**编译阶段全部通过、零错误零警告**，
但进入链接阶段（`Linking CXX executable SketchCAD.exe`）时崩盘，报错形如：

```
undefined reference to `SketchCAD::LayerNaming::checkLayerNaming(QList<QString> const&)'
undefined reference to `SketchCAD::LayerNaming::isLayerNameCompliant(QString const&, QString*)'
undefined reference to `SketchCAD::LayerNaming::suggestLayerNames()'
undefined reference to `SketchCAD::LayerNaming::standardLayerEntries()'
```

第一直觉很容易误判：**"是不是这 4 个函数没被编译进目标文件？"**
——但编译日志里明明有 `Building CXX object .../Layer.cpp.o`，且 `nm` 能在该 .o 里看到这 4 个符号，
只是符号的**名字修饰（name mangling）路径不对**。

---

## 2. 假设空间（Hypothesis Space）

链接器报 `SketchCAD::LayerNaming::checkLayerNaming`，而调用方（ComplianceChecker.cpp、
LayerPanel.cpp、Commands.cpp）都按 `LayerNaming::` 去引用，说明：

| 候选假设 | 方向 | 可能性 |
|---|---|---|
| ① 函数根本没定义 | 缺 .cpp / 没进 CMake | 低（编译过了，.o 存在） |
| ② 函数定义了，但符号名（修饰后）和声明对不上 | 命名空间 / 签名不一致 | **高** |
| ③ 模板/inline 误用导致 ODR 问题 | 头文件 | 低（全是普通函数） |
| ④ 调用约定 / extern "C" 错配 | 平台 ABI | 低（纯 C++ 普通函数） |

最值得怀疑的是 **②：声明在 `SketchCAD::LayerNaming` 命名空间，定义却落在了别处**。

---

## 3. 排查路径（Debugging Trail）

### 3.1 先看"声明"长什么样

`src/core/Layer.h` 末尾（在 `standardLayers()` 与 `} // namespace SketchCAD` 之间）写的是：

```cpp
namespace LayerNaming {
    std::vector<StandardLayerEntry> standardLayerEntries();
    QStringList suggestLayerNames();
    bool isLayerNameCompliant(const QString& name, QString* reason = nullptr);
    QVector<LayerNamingIssue> checkLayerNaming(const QStringList& names);
} // namespace LayerNaming
```

注意：`LayerNaming` 被包在 `namespace SketchCAD { ... }` 之内，
所以**声明的真实全名是 `SketchCAD::LayerNaming::*`**。

### 3.2 再看"定义"长什么样（初版错误版）

初版 `src/core/Layer.cpp` 写成了：

```cpp
#include "core/Layer.h"
#include "core/standards/StandardsDB.h"

namespace SketchCAD {
namespace {                         // 匿名命名空间：matchStandard 放这里，没问题
bool matchStandard(const QString& n, const StandardLayerEntry& e) { ... }
} // namespace

// ❌ 错误：4 个函数直接定义在 SketchCAD 下，没有再嵌 LayerNaming
std::vector<StandardLayerEntry> standardLayerEntries() { ... }
QStringList suggestLayerNames() { ... }
bool isLayerNameCompliant(const QString& name, QString* reason) { ... }
QVector<LayerNamingIssue> checkLayerNaming(const QStringList& names) { ... }

} // namespace SketchCAD
```

### 3.3 根因定位（Root Cause）

**头文件把 4 个函数声明在嵌套命名空间 `SketchCAD::LayerNaming`，
而 .cpp 把它们定义在了上一层 `SketchCAD`（未嵌套 `LayerNaming`）。**

后果：

- 编译器阶段：**不报错**。因为 `SketchCAD` 命名空间下确实"看见"了 4 个同名函数定义，
  语法、签名自洽，编译通过、产出 `Layer.cpp.o`。
- 链接器阶段：**找不到 `SketchCAD::LayerNaming::*`**。
  调用方引用的是带 `LayerNaming` 全名的修饰符号，
  而 `.o` 里实际导出的是 `SketchCAD::standardLayerEntries`（缺了一层），
  两边 name mangling 路径不一致 → `undefined reference`。

这是一个**典型的"编译期静默、链接期爆炸"陷阱**：
C++ 的命名空间层级是符号真名的一部分，少嵌一层就等于定义了另一个函数。

### 3.4 为什么初版会漏掉这层？

复盘主观原因，有两点值得记取：

1. **"头文件新加的命名空间"容易被 .cpp 作者忽略**。
   头里 `namespace LayerNaming { ... }` 嵌在 `SketchCAD` 内；
   作者写 .cpp 时只记得"在 `SketchCAD` 里写实现"，顺手把函数铺平到 `SketchCAD` 下，
   没意识到头里还多裹了一层 `LayerNaming`。
2. **GCC 对"声明在 A::B、定义在 A"不会给任何警告**。
   `[-Wall -Wextra]` 零警告通过，让这个错误一直潜伏到链接才暴露——
   而这正好撞上本项目另一条铁律：**验收必须链接两个 exe 通过才算数**，
   否则这个 bug 会在"编译过了就以为好了"的错觉里溜进交付。

---

## 4. 修复要点（Fix）

把 4 个函数**包回与声明完全一致的嵌套命名空间**：

```cpp
#include "core/Layer.h"
#include "core/standards/StandardsDB.h"

namespace SketchCAD {

namespace {                         // 匿名：辅助函数 matchStandard，保持 internal linkage
bool matchStandard(const QString& n, const StandardLayerEntry& e) {
    const QString nm = e.name;
    return n == e.code + QStringLiteral("_") + nm
        || n == e.code + QStringLiteral(" ") + nm
        || n == e.code + QStringLiteral("-") + nm
        || n == e.code + nm
        || n == nm;
}
} // namespace

namespace LayerNaming {             // ✅ 必须与头文件声明嵌套层级完全一致
std::vector<StandardLayerEntry> standardLayerEntries() { ... }
QStringList suggestLayerNames() { ... }
bool isLayerNameCompliant(const QString& name, QString* reason) { ... }
QVector<LayerNamingIssue> checkLayerNaming(const QStringList& names) { ... }
} // namespace LayerNaming

} // namespace SketchCAD
```

**关键校验点**：`.cpp` 里每个公开函数的"命名空间路径"必须逐层等于 `.h` 声明。
本项目约定：头文件若用 `namespace A { namespace B { ... } }`，
则 .cpp 必须用 `namespace A { namespace B { ... } }` 镜像，不能用 `namespace A { ... }` 后裸写。

---

## 5. 验证（Verification）

修复后做三道关，全部通过才算收工：

1. **增量构建零警告**：`cmake --build build --target SketchCAD SketchSelfTest -j 1`
   → `Linking CXX executable SketchSelfTest.exe` 成功，`build exit=0`、`warning count = 0`。
2. **自测全量通过**：`QT_QPA_PLATFORM=offscreen ./build/SketchSelfTest.exe`
   → **通过 3342 / 失败 0，EXIT=0**（`t_layerGB` 18 子项并入全量，由 3324→3342）。
3. **dist 字节同步**：`cmp build/SketchCAD.exe dist/SketchCAD.exe`
   → `SYNC OK: build/dist byte-identical`（已 `cp` 同步，消除 dist 旧 stale 残留）。

> 顺带暴露并修正的第二个隐患：**dist 目录里的 `SketchCAD.exe` 是旧时间戳（13:48）的 stale 副本**，
> 即便 build 修好了，若忘了 `cp`，用户跑到的仍是旧 exe。本项目的铁律"改源码后必须 `cmp` 同步 dist"即源于此。

---

## 6. 关联陷阱：全量重编的代价

排障过程中还有一条经验需要记录：

- **改动 `Layer.h`（被大量文件 include）会触发接近全量重编**，
  本次全量约 **26 分钟**（受限于 `-j1` 防 OOM 约束）。
- 第一次是改 `Layer.h` 引出的全量，第二次**只改 `Layer.cpp`**（不动头），
  则编译仅 6 步增量、迅速通过。

**启示**：命名空间这类"只动 .cpp 不动 .h"的修复，应尽量收口在 .cpp 内，
避免为修一个链接符号去改公共头，从而把 26 分钟的全量代价压到几秒的增量。

---

## 7. 经验固化（Lessons Learned）

| 要点 | 规则 |
|---|---|
| 命名空间配平 | 头声明在 `A::B`，.cpp 定义必须 `namespace A { namespace B { ... } }` 镜像，禁止在 `A` 下裸写 |
| 编译零警告 ≠ 链接通过 | `-Wall -Wextra` 对"声明/定义命名空间不一致"**不告警**；必须以"两 exe 链接成功 + 自测 EXIT=0"为验收硬门槛 |
| dist 必须同步 | 构建完成后 `cmp build/<exe> dist/<exe>`，不一致即 `cp`；dist 旧副本会让修复"看不见" |
| 收口改动范围 | 链接符号类修复尽量只动 .cpp，避免改公共头触发全量重编（~26min vs 数秒增量） |
| 链接错误先看符号修饰 | `undefined reference to A::B::f` 而编译通过时，第一直觉应是"定义与声明的命名空间/签名路径不一致"，而非"没编译" |

---

## 8. 小结

这次"修复"的本质不是业务逻辑错误，而是 **C++ 命名空间层级即符号真名** 这一语言特性，
在"头文件新加嵌套命名空间、.cpp 漏嵌一层"时制造的**编译期静默、链接期爆炸**陷阱。

排查思路可以提炼为一句话：
> 当编译全过、链接报 `undefined reference to A::B::x` 时，
> 不要怀疑"函数没编译"，而要怀疑"定义处的命名空间/签名路径 ≠ 声明处的路径"，
> 用 `nm` 看一眼 `.o` 里实际导出的符号名，路径差一层就一目了然。

该经验已同步写入项目记忆（`2026-11-12.md` 排坑段），作为后续相似场景的复用依据。
