# 商业安装包 —— 版本统一 · 文件属性 · 图标 · 许可协议 · 卸载入口

> 对应需求：产出可分发的商业安装包，带版本号、文件属性、图标、许可协议与卸载入口。
> 涉及路径：`CMakeLists.txt`、`resources/{app.rc.in,version.h.in,logo.ico}`、
> `LICENSE.txt`、`packaging/installer.iss`、`build.sh`、`README.md`、
> `src/ui/MainWindow.cpp`（仅「关于」对话框）、`tools/make_icon.py`。
> 改动边界：**只新增打包脚本与资源，不修改核心/UI 业务逻辑**；
> 不得将 Qt 运行时以外的新第三方 DLL 隐式带入。

---

## 0. 一句话结论

版本号只在 `CMakeLists.txt` 的 `project(SketchCAD VERSION x.y.z)` 写一次，
由此派生出「关于对话框」「exe 文件属性」「安装包文件名」三份产物；
`./build.sh package` 一条命令完成 Release 构建 → 汇集程序目录 → ISCC 编译 →
SHA-256 校验和，并**在打包末尾机器校验三者版本一致**，不一致直接失败。

---

## 1. 版本号：单一来源与三处派生

### 1.1 为什么必须收敛成一处

商用事故的经典形态是「版本号各说各话」：安装包叫 `1.1.0`、exe 属性是 `1.0.0`、
关于对话框写着 `1.2.0`。根因是**同一个版本号被抄了三遍**。
本项目此前已有同类教训（见记忆中的 DXF R2000、`writeDrawing` 序列化入口），
故本次直接采用「一个数字 + 三个派生」的结构：

```
        CMakeLists.txt: project(SketchCAD VERSION 1.1.0)
                            │
        ┌───────────────────┼───────────────────────┐
        ▼                   ▼                       ▼
configure_file          configure_file        build.sh sed 解析
version.h.in            app.rc.in             version.h
        │                   │                       │
        ▼                   ▼                       ▼
「关于」对话框          exe VERSIONINFO        /DAppVersion=1.1.0
（SKETCHCAD_VERSION）  （FILEVERSION）              │
                                                OutputBaseFilename
                                                → SketchCAD-1.1.0-Setup.exe
```

`build.sh` **不硬编码版本号**，而是从 `build/generated/version.h` 里 `sed` 解析
—— 那个文件本身就是 CMake 生成的，所以脚本读到的仍是同一个数字。

### 1.2 semver 三段 vs Windows 四段

| 出现位置 | 形式 | 说明 |
|---|---|---|
| 安装包文件名、关于对话框 | `1.1.0` | semver 三段，商用命名惯例 |
| exe 文件属性 VERSIONINFO | `1.1.0.0` | Windows 强制四段，第四段为修订号 |
| `project(VERSION)` | `1.1.0` | CMake 只接受三段 |

CMake 侧用一个变量派生两种写法，保证同源：

```cmake
set(SKETCHPAD_VERSION    "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.0")
set(SKETCHPAD_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
```

「关于」对话框**同时显示两者**（`版本 1.1.0（文件属性 1.1.0.0）`），
用户/客服比对时不会以为不一致。

### 1.3 打包末尾的机器自检

`build.sh package` 在产出后做一次断言（验收标准 ②）：

```
---- 版本一致性自检 ----
[OK] 版本一致：安装包名 1.1.0  |  exe 属性 1.1.0.0
```

校验「安装包文件名版本 == version.h 版本」且「app.rc 的 FILEVERSION == 期望四段式」。
任何一项不符即 `exit 1`，**不允许产出一个版本号自相矛盾的包**。
这比事后人工核对可靠 —— 打包是自动化的，人工核对必然会被跳过。

---

## 2. app.rc：图标 + 文件属性

### 2.1 只作用于主程序，不污染自测目标

```cmake
if(WIN32)
    enable_language(RC)
    configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc @ONLY)
    target_sources(SketchCAD PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc)
endif()
```

用 `target_sources` 而非全局命令：`SketchSelfTest` 不引入任何 `.rc`
（RC 编译需 `windres`，且 `.rc` 会拖入 GUI 资源依赖）。
这与既有「`SKETCHCAD_ENABLE_CRASHHANDLER` 宏只链 SketchCAD」是同一思路：
**双目标职责分离，自测目标保持纯净**。

### 2.2 中文不乱码的两个必要条件

```c
#pragma code_page(65001)      // ① 让 windres 按 UTF-8 解析本文件
BLOCK "040904b0"              // ② 用 Unicode StringFileInfo 块（65001 = UTF-16LE）
```

缺 ①  → 公司名/产品名按系统 ANSI（GBK）解析，源码是 UTF-8，结果是乱码；
缺 ②  → 用 `040904e4`（ANSI 块）存中文，在非中文 locale 的系统上同样乱码。

`Translation 0x0804, 1200` = 简体中文 Locale + Unicode 代码页。

实测验证（`Get-Item SketchCAD.exe | Select VersionInfo`）：

```
FileVersion     : 1.1.0.0
ProductVersion  : 1.1.0.0
CompanyName     : 航空CAD
ProductName     : SketchCAD 草图设计系统
FileDescription : SketchCAD 草图设计系统
LegalCopyright  : Copyright (C) 2026 航空CAD. All rights reserved.
```

### 2.3 windres 的三个语法坑

| 坑 | 现象 | 正解 |
|---|---|---|
| 版本号用点号 | `syntax error` | `FILEVERSION 1,1,0,0` —— **必须逗号分隔** |
| C 风格注释 | `syntax error` | `.rc` 的注释是 `//` 和 `/* */`；但注意不能在某些位置混用标点 |
| 中文按 ANSI 解析 | 乱码 | `#pragma code_page(65001)` |

第一点尤其隐蔽：同一个版本值在 `version.h` 里是字符串 `"1.1.0.0"`，
在 `.rc` 里却必须是裸数字 `1,1,0,0`。故 CMake 侧专门派生了一个 `_RC` 后缀变量，
而不是复用同一个 —— 这是**同一语义、两种语法**的典型情形。

---

## 3. 图标：零依赖生成多尺寸 ICO

`resources/logo.ico` 由 `tools/make_icon.py` 生成，**不引入 Pillow**
（本机 Python 无 PIL，且为一次性构建辅助，不值得加依赖）。

ICO 允许内嵌 BMP（DIB），脚本直接手写 `BITMAPINFOHEADER` + BGRA 像素 + 1bpp AND 掩码：

| 尺寸 | 编码 | 理由 |
|---|---|---|
| 16/24/32/48/64 | BMP + AND 掩码 | 小尺寸下 PNG 反而更大，且旧 shell 对 PNG 帧解析不佳 |
| 128/256 | PNG（zlib） | 大尺寸 PNG 压缩率显著更高（1.5KB vs BMP 的 16KB） |

**AND 掩码**是 BMP 形式 ICO 的必需项：位=1 表示透明。缺了它，图标会变成
「不透明的黑方块」—— 圆角和透明背景全丢。

图标语义：深蓝渐变圆角底 + 青色圆环 + 橙色十字（草图定位）+ 白色 "CAD" 字样，
5×7 点阵字模自绘。7 个尺寸共 38,684 字节。

> 为什么不直接用现成图标：本项目无品牌资产；自绘可保证多尺寸齐全且可复现
> （`python tools/make_icon.py` 一条命令重建）。有品牌资产后替换此文件即可，
> CMake 侧无需改动（路径由 `SKETCHICON_PATH` 变量提供）。

---

## 4. LICENSE.txt：商业许可

要点（完整文本见文件）：

- **专有商业软件**，非开源非免费；按台数授权；
- 禁止反向工程（仅保留「发布后 24 个月内排障性分析」这一窄豁免，且不包含
  源代码获取或商业利用）、再分发、转售、去保护；
- **输出成果归用户所有**，可自由商用 —— 这是商业 CAD 授权的关键卖点，
  必须在协议里明确写出；
- 第三方组件：Qt 6（**LGPL v3 动态链接**）、zlib、dbghelp，
  **均无强传染性 copyleft 义务**；明确说明 R2000 DXF 为自研、未用 GPL 的 libredwg；
- **用户数据在 `%LOCALAPPDATA%`，卸载不删除** —— 自动保存快照里可能有
  未保存的图纸，删掉等于丢稿。

---

## 5. installer.iss：安装器

### 5.1 页面与任务

| 环节 | 配置 | 要点 |
|---|---|---|
| 许可协议 | `LicenseFile=..\LICENSE.txt` | Inno 6 中 `AcceptLicense` **已被移除**且不建议加回：只要有 `LicenseFile`，向导必定显示协议页，且**默认选中「我不同意」**，必须显式改选「我同意」才能下一步 |
| 目标目录 | `DefaultDirName={autopf}\SketchCAD` | `autopf` 自动解析到 64 位 Program Files，避免 32 位重定向把文件塞进 `Program Files (x86)` |
| 开始菜单 | `{group}\...` + 卸载项 | `DisableProgramGroupPage=yes`（不单独出组选择页） |
| 附加任务 | `desktopicon` / `fileassoc`，**均默认不勾选** | 文件关联默认关：CAD 图纸常由别的 CAD 软件打开，抢关联是侵入行为 |
| 卸载注册表 | `HKLM\...\Uninstall\{AppId}_is1` | 补齐 Publisher / DisplayName / DisplayVersion / InstallLocation，便于企业资产管理 |
| 文件关联还原 | 每条 `Flags: uninsdeletekey` | **验收标准 ③ 要求**：卸载必须还原注册表 |

### 5.2 升级

同一 `AppId` ⇒ Inno 识别为「升级」而非并存，不会出现两个版本互相覆盖。
安装前 Inno 自动运行旧版 `unins000.exe`（`CloseApplications=yes` 会先提示关闭
正在运行的程序）。

**用户配置天然保留**，因为它本就不在安装目录内：

| 数据 | 位置 |
|---|---|
| 自动保存快照 / 崩溃转储 / 日志 | `%LOCALAPPDATA%\SketchCAD\{autosave,crashes,logs}` |
| 自动保存间隔等配置 | `HKCU\Software\SketchCAD` |

`[UninstallDelete]` 只清 `{app}\*.tmp` / `*.bak` / `translations`，
**刻意不碰 `%LOCALAPPDATA%`**。

### 5.3 静默安装（企业部署）

```cmd
SketchCAD-1.1.0-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
SketchCAD-1.1.0-Setup.exe /VERYSILENT /DIR="D:\Apps\SketchCAD" /NORESTART
SketchCAD-1.1.0-Setup.exe /VERYSILENT /TASKS="fileassoc,desktopicon" /NORESTART
```

`/VERYSILENT` 会自动接受许可协议 —— 故企业分发前应确保已按合同获得授权
（这一点在 README 与脚本注释里都写明了，避免合规风险）。

### 5.4 ISCC 的语法坑

| 坑 | 现象 | 正解 |
|---|---|---|
| C 风格注释 | `Expression expected but slash ("/") found` | ISPP 预处理器**只认 `;` 注释** |
| 动态 Source 目录 | 无法指向每次不同的打包目录 | `#ifndef StageDir` + `build.sh` 传 `/DStageDir=` |
| `AcceptLicense` | `Unrecognized [Setup] section directive` | Inno 6 已移除该指令，删掉即可 |

---

## 6. build.sh package：一条命令

```
Release 构建
  → 从 version.h 解析版本号
  → 探测 ISCC（PKG_ISCC → PATH → 常见路径 → Program Files 扫描）
  → 汇集到全新暂存目录 build/pkg-stage-$$-<版本>
  → ISCC 编译（/DAppVersion= /DStageDir=）
  → 版本一致性自检
  → sha256sum > dist/checksums.sha256
  → 打印产物路径与校验和
```

### 6.1 暂存目录：每次全新，脚本不删任何东西

初版用固定 `stage/` + `rm -rf` 清理。两个问题：

1. **正确性**：`stage/` 里的旧版残留文件会被一起打进安装包（升级时尤其明显）；
2. **可用性**：连续两次执行 `./build.sh package` 时，第二次要删 50 个文件，
   触发批量删除安全钩子被拦下 —— 也就是说**这个脚本第二次跑就会失败**。

改为 `build/pkg-stage-$$-<版本>`（`$$` 为进程号）后：
天然无残留，且**脚本一个文件都不用删**。这既绕开了安全钩子，也更安全
（整目录 `rm -rf` 一旦路径变量出错就是不可逆损失）。

### 6.2 deploy_qt：给 windeployqt 加一道结果校验

```bash
"$QT_ROOT/bin/windeployqt.exe" ... >/dev/null || true
if [ ! -f "$target_dir/Qt6Core.dll" ]; then
    echo "[错误] windeployqt 未能在 $target_dir 部署 Qt 运行时。" >&2
    ...  # 列出 3 类常见原因与处理
    exit 1
fi
```

**为什么要校验**：实测遇到过 windeployqt 在管道资源耗尽时
只打印一句 `Unable to query qtpaths: Error running binary qtpaths: pipe: `
（真实错误是 `CreateFile failed. 所有的管道范例都在使用中`），
然后**静默什么都不部署**。若不校验，产出的安装包会「装得上、启动即崩」——
这在商业分发里是事故。加了校验后，脚本明确失败并给出可操作提示。

`dist` 与 `package` 共用这一个函数，保证「装出来的」与「拷出去跑的」
是同一批二进制。

### 6.3 SKETCHCAD_QT_STAGE：显式 opt-in 的复用通道

某些环境（CI、交叉编译、容器/沙箱）无法运行 windeployqt。提供：

```bash
SKETCHCAD_QT_STAGE=/path/to/deployed ./build.sh package
```

**刻意不做的事**：不猜、不静默回退。源目录缺 `Qt6Core.dll` 就直接报错。
自动回退到某个"大概是上次构建产物"的目录，等于可能把**过期或被污染的 DLL**
打进商用安装包 —— 那是比构建失败严重得多的后果。

复用时**只取 Qt 运行时**（`*.dll` + `platforms/imageformats/styles/translations`）：

> 这里踩过坑：初版写的是 `cp -r "$src"/. "$target_dir"/`，而复用源指向 `dist/`，
> 结果把上一版安装包、PDF、日志全拷了进去 —— 包体从 16MB 涨到 30MB，
> 且出现「安装包里套着一个安装包」。**复用必须按白名单取，不能整目录搬。**

### 6.4 MSYS 参数转换

```bash
MSYS2_ARG_CONV_EXCL="*" MSYS_NO_PATHCONV=1 "$ISCC" "/DAppVersion=$APP_VERSION" ...
```

Git Bash 会把以 `/` 开头的参数当 POSIX 路径改写：
`/DAppVersion=1.1.0` → `C:/Program Files/Git/DAppVersion=1.1.0`，
ISCC 于是报 `You may not specify more than one script filename`。
关闭参数转换是标准做法（`MSYS_NO_PATHCONV` 为旧版兼容）。

---

## 7. 验收对照

| 验收标准 | 状态 | 证据 |
|---|---|---|
| ① 全新虚拟机安装 → 绘制/保存/打开 → 卸载无残留 | **待人工验证** | 见第 8 节清单。包体 16MB，含 Qt+MinGW 运行时，目标机无需装 Qt/编译器 |
| ② 关于 = exe 属性 = 安装包名 | ✅ | 三者同源；`[OK] 版本一致：安装包名 1.1.0 \| exe 属性 1.1.0.0`；实测 exe 属性为 `1.1.0.0` / 航空CAD |
| ③ `./build.sh package` 产出并打印校验和 | ✅ | `PKG_EXIT=0`；产出 `dist/SketchCAD-1.1.0-Setup.exe`(16MB) + `dist/checksums.sha256`；`sha256sum -c` 通过 |
| ④ `./build.sh dist` 行为不变 | ✅ | dist 改用同一个 `deploy_qt`，成功时产物完全一致；仅在 windeployqt 失败时由「静默产出坏包」改为「明确报错」 |
| 边界：不改业务逻辑 | ✅ | 仅改 `MainWindow.cpp` 的「关于」菜单回调（`model_->log` → `QMessageBox::about` + 版本宏），绘图/文件 IO 逻辑零改动 |
| 边界：不多带第三方 DLL | ✅ | 暂存目录 39 个文件：DLL 仅 `Qt6{Core,Gui,Network,PrintSupport,Qml,Widgets}` + MinGW 运行时 `libgcc_s_seh-1/libstdc++-6/libwinpthread-1`（均为 windeployqt 标准输出），无其他第三方库 |

回归：`./build.sh test` 全量通过（见运行记录），双目标零警告。

---

## 8. 人工验证清单（全新虚拟机）

1. 拷贝 `dist/SketchCAD-1.1.0-Setup.exe` 到**无 Qt、无编译器**的干净虚拟机；
2. 校验完整性：`sha256sum -c checksums.sha256`；
3. 双击安装 → 许可协议页**不选「我接受」时「下一步」应不可用**；
4. 目标目录默认 `C:\Program Files\SketchCAD`；
5. 勾选「关联 .json/.dxf」→ 完成；
6. 从开始菜单启动 → **帮助 → 关于** 确认版本 `1.1.0（文件属性 1.1.0.0）`；
7. `资源管理器` 右键 `SketchCAD.exe` → 属性 → 详细信息，确认图标与版本；
8. 画一条线 → `文件 → 保存` 到桌面 → 关闭 → 重新打开确认内容一致；
9. 再次运行**同版本**安装包 → 应识别为升级，不出现两个版本；
10. 控制面板 → 程序和功能 → 卸载 → 确认：
    - 程序目录已删除；
    - 桌面/开始菜单快捷方式已删除；
    - `.json` 文件关联**已还原**（右键菜单不再出现 SketchCAD）；
    - `%LOCALAPPDATA%\SketchCAD` **仍然存在**（用户数据刻意保留）；
11. 静默安装验证：
    `SketchCAD-1.1.0-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="D:\Apps\SketchCAD"`

---

## 9. 遗留与后续

- **未签名**：安装包与 exe 均未做 Authenticode 数字签名，企业环境会触发
  SmartScreen「未知发布者」警告。商用发布**强烈建议**购买代码签名证书
  （OV/EV），并在 CI 中用 `signtool sign` 步骤接入。
- **ISCC 非商业版**：当前用 Inno Setup 官方免费版，其编译器输出带
  `Non-commercial use only` 标识。**商业分发必须购买商业许可**，
  或改用 NSIS（MIT 许可，无此限制）。
- **未实现多语言**：`[Languages]` 目前只有 `chinese`（`Default.isl`）。
- **未实现组件拆分**：安装器是全量包，无法只装「核心 + 可选模板库」。
- **`SKETCHCAD_QT_STAGE` 会累积暂存目录**：`build/pkg-stage-*` 不会自动清理
  （`build/` 已在 `.gitignore` 内，随构建目录一起清即可）。
