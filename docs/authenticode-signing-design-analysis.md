# Authenticode 签名接入打包流水线 —— 设计分析

> 文档性质：设计决策与思考过程记录（本仓库惯例：`<feature>-design-analysis.md`）  
> 关联文档：`docs/inno-setup-licensing-evaluation.md`（安装器授权合规评估）  
> 涉及代码：`build.sh`（package 模式）、测试 `build/test-sign-file.sh`（一次性 harness，不入版本控制）  
> 日期：2026-10-09

---

## 1. 边界与现状

### 1.1 任务边界

在 `build.sh package` 流水线中接入 Authenticode 代码签名，覆盖四项需求：

1. ISCC 编译后增加签名步骤（`signtool sign /fd SHA256 /tr <时间戳>`）；
2. 对 `Setup.exe` 与包内 `SketchCAD.exe` **双签名**；
3. `checksums.sha256` 纳入**签名后**哈希；
4. 证书路径与口令走环境变量 `PKG_CERT` / `PKG_CERT_PWD`，缺失则跳过并醒目告警"产物未签名"。

### 1.2 改造前现状

```
cmake+ninja 构建 → 读版本号(单一来源) → 汇集 STAGE → ISCC 编译 → SETUP 存在性校验 → sha256 校验和 → 版本一致性自检 → 汇总输出
```

- `installer.iss` 无 `SignTool` 指令（曾核查），签名只能走 build.sh 侧独立调用 signtool；
- 本机环境探测结论：
  - signtool：`C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe`（**不在 PATH**，须显式探测）；
  - ISCC：`%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`（build.sh 候选列表第 5 项可命中）；
  - `PKG_CERT` / `PKG_CERT_PWD`：均未设（默认即"无证书环境"）。

### 1.3 硬约束（来自任务要求）

- 签名失败**不得**产出静默无签名的"看起来成功"包（告警必须可见）；
- 证书口令**绝不**写入任何脚本/仓库文件；
- 现有版本号单一来源与校验和逻辑**不动**（仅允许移动校验和的计算时机）。

## 2. 关键设计决策

### D1 —— 双签名的时机与位置（为什么不是 ISCC 内嵌签名）

| 方案                                | 评估                                                                            |
| --------------------------------- | ----------------------------------------------------------------------------- |
| ISCC `SignTool` 指令（仅能签 Setup.exe） | 只解决一半需求；且 signtool 不在 PATH 时 ISCC 内部签名报错语义不透明                                 |
| **build.sh 侧两次显式调用（选定）**          | 完整覆盖"包内 exe + Setup.exe"双签名；每步成败可显式裁决；与既有 MSYS 参数转换坑（`MSYS2_ARG_CONV_EXCL`）解耦 |

落点顺序：

```
STAGE/SketchCAD.exe 签名（ISCC 嵌入前，预签名）
    ↓ ISCC 把已签名副本原样打进安装包
Setup.exe 签名（ISCC 编译后）
    ↓
checksums.sha256（签名后计算）
```

**关键点：包内 exe 必须在 ISCC 之前签名。** ISCC 是把 STAGE 目录字节级打进安装包，编译后再签"包内"文件是不可能的；因此采用"预签名 STAGE 副本"方案。最终用户安装出来的 `SketchCAD.exe` 自带 Authenticode 签名。

### D2 —— 失败裁决矩阵（核心决策）

| 环境状态                           | 行为                                           | 退出码     | 设计理由                                             |
| ------------------------------ | -------------------------------------------- | ------- | ------------------------------------------------ |
| `PKG_CERT` 未设                  | 跳过签名，stderr 打印 `!!!!!!` 包围的醒目告警"产物未签名"+ 配置指引 | 0（继续打包） | 验收要求"无证书环境跑流水线得到明确告警且包体正常"；开发/内部构建不应被签名阻塞        |
| `PKG_CERT` 已设但缺 `PKG_CERT_PWD` | 报错中止                                         | 1       | 证书给了口令没给，必然失败；早失败优于编译完才失败                        |
| 已设但找不到 signtool                | 报错中止 + 提示 `PKG_SIGNTOOL`                     | 1       | 同上                                               |
| 已设但 `signtool sign` 返回非 0      | 报错中止（"已中止打包，避免产出无签名安装包"）                     | 1       | **约束核心**：签名失败 = 产物缺失签名，静默继续会产出"看起来成功"的无签包，商业分发事故 |
| 已设且签名成功                        | 打印 `[OK] 已签名并通过 verify /pa`                  | 继续      | —                                                |

`exit 1` 位于函数内，配合脚本头部 `set -euo pipefail` 立即终止整条流水线，任何后续步骤（校验和、汇总框）都不会执行——不存在"签坏了但校验和照算"的中间态。

### D3 —— 口令安全（绝不落盘）

- 口令**唯一**入口：环境变量 `PKG_CERT_PWD`，脚本内仅以 `"$PKG_CERT_PWD"` 展开传给 signtool 的 `/p` 参数；
- 脚本回显刻意写 `/f <PKG_CERT>`（证书路径非秘密）而**不回显口令**；单测场景 2e 专门断言回显中不含口令字面量；
- 仓库内 grep 无任何口令字面量；mock 单测中口令出现在临时日志属测试自身行为，不属仓库。

已知残留面：signtool 进程命令行中含 `/p <口令>`，同机其他进程可经进程快照窥见（signtool 无 stdin 传口令的稳定机制；替代方案是证书导入系统证书库后用指纹 `/sha1`，见 §8）。

### D4 —— 校验和后置

`sha256sum` 语句**原样保留**（满足"校验和逻辑不动"），仅物理位置从"ISCC 之后"移到"双签名之后"。理由：签名会改写 PE 文件字节（附加签名表 + 校验和字段），先算后签必然失配。产物摘要因此是**签名后**的，下游 `sha256sum -c checksums.sha256` 校验的是用户实际拿到的文件。

### D5 —— `signtool verify /pa` 的定位：信息性自检

签名成功后脚本自动跑一次 `verify /pa`，**失败仅告警不中止**。理由：

- `signtool sign` 返回 0 已保证签名有效嵌入；
- `verify /pa` 额外要求**构建机**信任完整证书链（根 CA + 中间 CA），这是构建机环境属性而非产物属性——构建机缺中间 CA 时产物在用户机上仍可正常验证；
- 真正的验收由人工在签名机上执行 `signtool verify /pa` 完成（见 §6.3），该口径与任务验收条款一致。

### D6 —— 时间戳服务器可配置

默认 `http://timestamp.digicert.com`（公共免费 RFC3161 服务），可用 `PKG_TIMESTAMP_SERVER` 覆盖（企业内网/国内镜像场景）。签名同时带 `/fd SHA256`（摘要算法）与 `/td SHA256`（时间戳摘要算法，RFC3161 惯例配套）。

## 3. 实现落点（build.sh 共 5 处编辑）

| # | 位置                        | 内容                                                                                                                                 |
| - | ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| 1 | 文件头用法注释                   | `package` 模式说明补签名一行                                                                                                                |
| 2 | package 段头注释              | 流程图更新为"…包内 exe 预签名 → ISCC 编译 → Setup.exe 签名 → sha256 校验和（签名后哈希）"                                                                   |
| 3 | ISCC 探测后（`[OK] ISCC:` 之后） | **新增 `sign_file()` 函数**（约 47 行）：证书缺失分支 → 口令缺失分支 → signtool 探测（`PKG_SIGNTOOL` → PATH → 两个默认 SDK 路径）→ sign（失败 exit 1）→ verify（失败仅告警） |
| 4 | staging 就绪之后、ISCC 之前      | `sign_file "$STAGE/SketchCAD.exe"`（包内预签名）                                                                                          |
| 5 | SETUP 存在性校验之后             | `sign_file "$SETUP"`（Setup 签名）；紧接其后为**移入**的 checksums 语句；汇总框新增"签名"状态行（已双签名 / 【未签名】+ 指向告警）                                          |

`sign_file` 探测顺序与 ISCC 探测同构（环境变量 → PATH → 常见路径），保持脚本内一致性。

## 4. 环境变量一览

| 变量                     | 必需              | 作用                        | 默认                                         |
| ---------------------- | --------------- | ------------------------- | ------------------------------------------ |
| `PKG_CERT`             | 否               | PFX 证书文件路径；未设=跳过签名（告警后继续） | （空）                                        |
| `PKG_CERT_PWD`         | 设了 PKG_CERT 则必需 | PFX 口令；仅经环境变量注入           | （空）                                        |
| `PKG_SIGNTOOL`         | 否               | signtool.exe 完整路径覆盖       | 依次探测 PATH、Windows SDK 10.0.26100.0、SDK x64 |
| `PKG_TIMESTAMP_SERVER` | 否               | RFC3161 时间戳服务器            | `http://timestamp.digicert.com`            |
| `PKG_ISCC`             | 否               | （既有）ISCC 路径覆盖             | 既有探测链                                      |

启用签名示例（口令只在 shell 会话内，不落盘）：

```bash
export PKG_CERT='/d/certs/sketchcad-codesign.pfx'
read -rs PKG_CERT_PWD && export PKG_CERT_PWD   # 交互式输入不回显
./build.sh package
# 人工验收：
"/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/signtool.exe" verify /pa dist/SketchCAD-1.1.0-Setup.exe
```

## 5. 约束映射核对

| 任务约束      | 实现                                       | 落点                                          |
| --------- | ---------------------------------------- | ------------------------------------------- |
| 口令不入脚本/仓库 | 仅环境变量；回显脱敏；单测场景 2e 断言                    | `sign_file()` 回显行、`build/test-sign-file.sh` |
| 签名失败不静默出包 | sign 非 0 → `exit 1`（流水线终止）               | D2 矩阵                                       |
| 无证书环境可出包  | 告警分支 `return 0`                          | D2 矩阵                                       |
| 告警可见      | stderr + `!!!!!!` 框 + 配置指引 + 汇总框【未签名】状态行 | 编辑 #3、#5                                    |
| 版本号单一来源不动 | 未触碰 VER_HDR 解析与版本自检段                     | —                                           |
| 校验和逻辑不动   | `sha256sum` 语句原样，仅移位                     | D4                                          |

## 6. 验证方案与证据

### 6.1 语法检查

`bash -n build.sh` → `SYNTAX_OK`（EXIT=0）。

### 6.2 `sign_file` 四场景 mock 单测（build/test-sign-file.sh）

方法：`sed` 从 build.sh 提取函数体（去 4 空格缩进）独立 source；mock signtool 以 `PKG_SIGNTOOL` 注入。**结果 8/8 PASS**：

| 场景               | 断言                                                                 | 结果   |
| ---------------- | ------------------------------------------------------------------ | ---- |
| 1 无证书            | rc=0 且输出含"未签名"告警                                                   | PASS |
| 2 证书齐全 + mock 成功 | rc=0 且输出含 verify /pa                                               | PASS |
| 2b               | 目标文件被 mock 写入 FAKESIGNATURE                                        | PASS |
| 2c               | 实参含 `sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256` | PASS |
| 2d               | 实参含 `/f <证书路径>` 与 `/p <口令>`                                        | PASS |
| 2e               | 脚本回显不含口令字面量                                                        | PASS |
| 3 mock 签名失败      | rc=1 且输出含"已中止打包"                                                   | PASS |
| 4 有证书缺口令         | rc=1 且输出含"缺少 PKG_CERT_PWD"                                         | PASS |

（测试基建坑：本机 `mktemp` 返回混合分隔符路径，反斜杠破坏 grep 模式 → harness 临时目录改放 `build/` 下。）

### 6.3 端到端实测（无证书环境，真实流水线）

`unset PKG_CERT PKG_CERT_PWD; ./build.sh package` 真实运行（cmake+ninja 全量重建 + ISCC 编译）：

- 预期：两处 `[警告] …未签名` 可见告警、包体正常产出、checksums 与产物一致、`EXIT=0`。
- 实测结果见文末 §9 验收证据回填。

### 6.4 待人工验收（需真实证书，本机无证书无法自动化）

```bash
signtool verify /pa dist/SketchCAD-<版本>-Setup.exe    # 双签名产物
signtool verify /pa "<安装目录>/SketchCAD.exe"          # 包内签名（安装后）
( cd dist && sha256sum -c checksums.sha256 )            # 校验和 = 签名后哈希
```

## 7. 已知限制与风险

1. **口令经进程命令行可见**（§3 已述）：高安全要求场景建议改用证书库 + `/sha1 <指纹>`（导入一次 PFX 到当前用户证书库后，signtool 不需要口令）。当前实现保留升级路径：仅 `sign_file` 内 sign 一行需改。
2. **EV 证书**：私钥在硬件 token 的 EV 证书通常弹出私钥授权窗，CI 无人值守场景需厂商专用签名服务（如 Azure Trusted Signing / DigiCert KeyLocker），届时在 `sign_file` 内替换 sign 调用即可，双签名时序与裁决矩阵不变。
3. **时间戳服务可用性**：依赖公网可达 digicert；不可达时签名仍成功但**无时间戳**——证书过期后签名即失效。建议发行前人工确认 `PKG_TIMESTAMP_SERVER` 可达。
4. **verify /pa 构建机依赖**：见 D5，产物无恙、告警属构建机链问题。

## 8. 结论

- 签名逻辑收敛为单一入口 `sign_file()`，两处调用点（包内预签 + Setup 后签）复用同一裁决矩阵，杜绝"一处更新一处漏"；
- 失败语义三分：无证书=告警继续（EXIT=0）、配置缺陷=早失败（EXIT=1）、签名失败=中止不出包（EXIT=1），与"告警必须可见 / 不得静默无签"约束一一对应；
- 校验和后置使 `checksums.sha256` 始终对应签名后产物；版本单一来源与校验和格式零改动；
- 安装器授权合规（Inno Setup 6.5 商业许可请求）独立评估，见 `docs/inno-setup-licensing-evaluation.md`。

## 9. 验收证据（端到端实测回填，2026-10-09）

### 9.1 无证书环境完整流水线（真实运行）

命令：`unset PKG_CERT PKG_CERT_PWD; ./build.sh package`（首次运行因沙箱 windeployqt 管道问题中止于部署守卫——守卫按设计拦截 EXIT=1，复用 `SKETCHCAD_QT_STAGE=<旧 stage 目录>` 后重跑成功）。

关键日志（`build/package-nocert2.log`）：

```
18:  [警告] 未设置 PKG_CERT —— 产物 SketchCAD.exe **未签名**！          ← 包内预签名点（ISCC 之前）
157: [OK] ISCC 编译完成
160: [警告] 未设置 PKG_CERT —— 产物 SketchCAD-1.1.0-Setup.exe **未签名**！ ← Setup 签名点（ISCC 之后）
170: ==================== 商业安装包已产出 =====================
175:   签名     : 【未签名】本次未设置 PKG_CERT，产物无数字签名（详见上方告警）
180: EXIT=0
```

- 两处告警均以 `!!!!!!!!` 框包围输出到 stderr，`grep -c "未设置 PKG_CERT"` = 3（2 告警 + 1 汇总框）；
- 产物正常：`dist/SketchCAD-1.1.0-Setup.exe`（16,241,621 字节）；
- **全程 `EXIT=0`** ✅（满足验收"无证书环境得到明确告警且包体正常"）。

### 9.2 校验和与签名后产物一致

```
$ cd dist && sha256sum -c checksums.sha256
5aec219a13cce5c242fdadc032a3110f6c7b0eeaa62008dd3c4cfa442ae7e9a0 *SketchCAD-1.1.0-Setup.exe
SketchCAD-1.1.0-Setup.exe: OK
```

校验和位于双签名步骤之后计算，即"签名后哈希"位置正确（本机无证书故哈希对象为未签名产物；签名路径的哈希一致性由 §6.2 场景 2 的执行顺序 + 单元断言保证）。

### 9.3 验收结论对照

| 验收条款                              | 结果                                |
| --------------------------------- | --------------------------------- |
| 无证书环境跑流水线得到明确告警且包体正常              | ✅ 实测（9.1）                         |
| `checksums.sha256` 与签名后文件一致       | ✅ 实测（9.2，`sha256sum -c` OK）       |
| `build.sh package` 全程 EXIT=0（无证书） | ✅ 实测（9.1）                         |
| 双签名产物 `signtool verify /pa` 通过    | ⏳ 待真证书人工验收（§6.4；逻辑由 §6.2 四场景单测覆盖） |
| 签名失败不静默出包 / 口令不入库                 | ✅ 单测场景 2e/3/4（8/8 PASS）           |
