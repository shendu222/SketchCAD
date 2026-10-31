# 第三方依赖与许可声明（Third-Party Notices）

> 本文件随 SketchCAD 安装包分发（安装目录 `third-party-notices/`），说明产品所含第三方组件的
> 名称、版本、许可证、分发方式与源码获取途径。任何第三方组件的增删都必须在合入前更新本文件。

## 1. GNU LibreDWG（DWG 转换组件，GPLv3）

SketchCAD 的 DWG **R13–R2018 读取**与**R2000 写出**能力由 GNU LibreDWG 提供的两个命令行
转换器承担：

| 组件 | 作用 | 位置 |
|---|---|---|
| `dwg2dxf.exe` | DWG（AC1012–AC1032）→ DXF，供 SketchCAD 回读 | `<安装目录>\tools\dwg\` |
| `dwgwrite.exe` | DXF → DWG R2000（AC1015） | `<安装目录>\tools\dwg\` |

| 项目 | 内容 |
|---|---|
| 组件名称 | GNU LibreDWG |
| 版本 | 0.14 |
| 许可证 | **GNU General Public License v3.0（GPLv3）** |
| 许可证全文 | `<安装目录>\COPYING`，另在 `<安装目录>\tools\dwg\COPYING` 随转换器就近附带 |
| 官方项目主页 | https://www.gnu.org/software/libredwg/ |
| 源码获取地址 | https://github.com/LibreDWG/libredwg |
| 上游镜像（GNU Savannah） | https://savannah.gnu.org/projects/libredwg/ |
| 本项目使用的构建方式 | MinGW-w64 + GCC 13.1 独立编译为两个控制台可执行文件 |

**GPLv3 合规说明。** GPLv3 的源码提供义务（§6）由「随二进制分发 + 公开源码获取地址」共同满足：
安装包内已附带 GPLv3 许可证全文，源码可从上述地址无偿获取；如需与本安装包**完全对应**的构建
源码快照，可依上述地址检出 `0.14` 标签后，用 MinGW-w64 自行编译，命令见 LibreDWG 仓库
`README` 的 `./configure --enable-write && make`。

**分发形态：独立进程，零链接。** SketchCAD 主程序**不链接** libredwg（无 `-ldwg`、无
`libredwg*.dll`、不含任何 LibreDWG 头文件派生的代码）。主程序与转换器之间只通过
**命令行参数 + 磁盘文件 + 退出码**交互，不存在共享内存、进程内调用或 ABI 耦合。因此
SketchCAD 自身不构成 LibreDWG 的衍生作品，其自有代码不受 GPLv3 传染；GPLv3 义务仅覆盖
`tools\dwg\` 目录下的两个独立可执行文件。

**可替换性。** 正因为是独立进程，用户可依 GPLv3 授予的自由，用自行编译或任何兼容版本的
LibreDWG 二进制替换 `tools\dwg\dwg2dxf.exe` 与 `dwgwrite.exe`，无需重新编译或修改 SketchCAD。
替换后的目录可通过以下任一方式指定：

1. 环境变量 `SKETCHCAD_DWGTOOLS` 指向含两个 exe 的目录；
2. 程序内 **选项 → 文件位置 → 「DWG 转换组件路径」**（持久化键 `files/dwgToolsOverride`，
   优先级高于环境变量，启动时回写为 `SKETCHCAD_DWGTOOLS`）。

**组件缺失时的行为。** 若未找到转换器，`tools\dwg` 相关功能降级而非静默失败：

- 打开 DWG R13–R2018 文件会明确报错并弹窗，报文含**文件版本串原文**（如 `AC1015`）与
  「未安装 DWG 转换组件」提示，附「查看帮助」链接；程序不崩溃。
- 另存为 DWG 时自动回落到**自研 R12（AC1009）原生写出**，并提示已降级。
- DWG **R12（AC1009）读取**始终由自研解析器完成，不依赖本组件。

> 自研路径（DXF 全版本读写、R12 原生 DWG 读写）与 LibreDWG 完全解耦：卸载 `tools\dwg`
> 后这些能力不受任何影响。

## 2. 自有实现范围（无第三方解析/几何库）

除上述**可选的**外部转换组件外，文件格式与几何内核均为本项目自研：

- DXF R12 / R2000 / R2004 / R2007 / R2010 / R2013 / R2018 读写（ASCII 与二进制 DXF 容器）；
- DWG R12（AC1009）二进制原生读写；
- 二维约束求解、布尔、填充、块与布局等几何能力。

## 3. 其他运行时依赖

| 组件 | 许可证 | 链接方式 | 传染性说明 |
|---|---|---|---|
| Qt 6.10.1 | LGPL-3.0（含例外） | 动态链接（DLL，随安装包分发） | LGPL 允许闭源商用；已动态链接且随包附带许可声明，用户可替换 Qt DLL |
| MinGW-w64 / GCC 13.1 运行时（`libstdc++-6.dll` 等） | GPLv3 + GCC Runtime Library Exception | 动态链接 | GCC 运行时例外明确允许闭源程序使用，不触发传染 |
| Inno Setup（仅打包期） | 自定义许可 | 不随产品分发 | 仅用于生成安装程序 |

## 4. 未采用的候选内核（评估记录）

| 候选库 | 许可 | 结论 |
|---|---|---|
| ODA Drawings SDK (Teigha) | 商业授权（须签约） | 未采用：商业授权成本与接入范围超出当前需求 |
| libdxfrw | GPL-2.1+ / 自定义 | 未采用：不能写 DWG，且许可受限 |
| dxflib (QCAD) | GPL-2.0 | 未采用：仅 DXF，许可受限 |

## 5. 合规检查清单（每次发版前）

1. `dist\tools\dwg\COPYING` 与 `<安装目录>\COPYING` 均存在，且与 LibreDWG 上游 `COPYING` 字节一致；
2. 主程序无 `libredwg` 链接（`objdump -p SketchCAD.exe | grep -i dwg` 应无 libredwg 依赖）；
3. 本文件随包分发，源码获取地址可访问且指向所用版本；
4. 若升级 LibreDWG 版本，同步更新本文档 §1 的版本号与构建方式描述。
