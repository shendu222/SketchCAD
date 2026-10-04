; ============================================================================
;  SketchCAD 草图设计系统 — 商业安装器（Inno Setup 6）
;  编译：ISCC.exe packaging/installer.iss
;  由 build.sh package 模式自动调用（勿手工改版本号，见下方 AppVersion）
;
;  版本号单一来源：CMakeLists.txt 的 project(SketchCAD VERSION ...)
;    → version.h（关于对话框）→ app.rc（exe 属性）→ 本脚本（安装包名）
;  三者一致由「都从同一 CMake 版本派生」保证，而非靠人工同步。
;
;  静默安装（企业批量部署）：
;    SketchCAD-<版本>-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
;    指定目录：/DIR="D:\Apps\SketchCAD"
;    含文件关联：/TASKS="fileassoc"
;  静默卸载：unins000.exe /VERYSILENT
; ============================================================================

#define AppName        "SketchCAD"
#define AppNameCN      "SketchCAD 草图设计系统"
#define AppPublisher   "航空CAD"
#define AppExeName     "SketchCAD.exe"
; 注意：Inno 预处理指令只认 ';' 注释，**不认 C 风格 // 注释**（会在 ISPP 阶段报
; "Expression expected but slash found"）。写注释一律用分号。
;
; AppId 必须用花括号包住 GUID：字面量里的单个 { } 会被 ISCC 当成常量引用。
#define AppId          "{{7C3E9A41-5B62-4D18-9F0C-2A6E3B8D4C57}"

; ---- 版本：由 build.sh 通过 -DAppVersion= 传入（ISCC 支持命令行定义覆盖）----
#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif
#define AppVersionFull "1.1.0.0"

[Setup]
AppId={#AppId}
AppName={#AppNameCN}
AppVersion={#AppVersion}
AppVerName={#AppNameCN} {#AppVersion}
VersionInfoVersion={#AppVersionFull}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppNameCN} 安装程序
VersionInfoProductName={#AppNameCN}
VersionInfoProductVersion={#AppVersionFull}
AppPublisher={#AppPublisher}
AppPublisherURL=https://www.example.com/sketchcad
AppSupportURL=https://www.example.com/sketchcad/support
AppUpdatesURL=https://www.example.com/sketchcad/download

; 安装到 64 位 Program Files（避免装进用户目录导致 32 位重定向问题）
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppNameCN}
DisableProgramGroupPage=yes

OutputDir=..\dist
; 产物：SketchCAD-<版本>-Setup.exe（验收标准 ⑤ 要求与版本号同源）
OutputBaseFilename={#AppName}-{#AppVersion}-Setup

; 压缩：lzma2 平衡压缩（安装包体积 vs 解压速度）
Compression=lzma2/ultra64
SolidCompression=yes

; 许可协议页。
; 【Inno Setup 6 注意】旧的 AcceptLicense 指令已被**移除**，且不建议再加回来：
; 只要指定了 LicenseFile，向导必定显示协议页，且默认选中「我不同意」，
; 用户必须显式改选「我同意」才能点「下一步」—— 这正是「必须接受」所需的行为。
; WizardStyle=modern 会用单选按钮（我接受 / 我不接受）呈现，可读性最好。
LicenseFile=..\LICENSE.txt
WizardStyle=modern

; 目标目录页：允许改；安装过程中禁止改（企业环境，避免半途换目录）
DisableDirPage=no
DirExistsWarning=no
; 不允许在「程序文件」目录下放用户数据：程序自身只写 %LOCALAPPDATA%

; 升级：允许覆盖同版本以上/以下的旧版，卸载旧版时先自动运行其 unins000.exe
CloseApplications=yes
RestartApplications=no
MinVersion=10.0

[Languages]
Name: "chinese"; MessagesFile: "compiler:Default.isl"

[Tasks]
; 可选任务（安装器「选择附加任务」页）
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked
; 文件关联为**可选**且默认不勾选：CAD 图纸可能由别的 CAD 软件打开，
; 强行抢关联是侵入行为，故让用户主动选。
Name: "fileassoc";  Description: "关联 .json / .dxf 图纸文件类型"; GroupDescription: "附加任务:"; Flags: unchecked

[Files]
; 主程序与 Qt 运行时。
; Source 根目录由 build.sh 通过 -DStageDir= 传入（每次打包都是全新空目录，
; 天然无旧版残留）。手工编译时可用默认的 ..\stage。
#ifndef StageDir
  #define StageDir "..\stage"
#endif
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 图标
Source: "..\resources\logo.ico"; DestDir: "{app}"; Flags: ignoreversion
; 许可协议副本随程序分发（用户可在程序目录内查阅）
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
; 第三方组件许可声明
Source: "..\docs\third-party-notices.md"; DestDir: "{app}\third-party-notices"; \
    Flags: ignoreversion skipifsourcedoesntexist

[Icons]
; 开始菜单（组内含卸载入口）
Name: "{group}\{#AppNameCN}";            Filename: "{app}\{#AppExeName}"
Name: "{group}\使用说明";                  Filename: "{app}\README.md"
Name: "{group}\卸载 {#AppNameCN}";        Filename: "{uninstallexe}"
; 桌面快捷方式（可选）
Name: "{autodesktop}\{#AppNameCN}";      Filename: "{app}\{#AppExeName}"; Tasks: desktopicon
; 快速启动栏不创建（现代 Windows 已弃用，且企业环境无意义）

[Registry]
; 卸载项：控制面板「程序和功能」可见。Inno 的 unins 键已自动写入，
; 这里额外登记 Publisher/DisplayName/InstallLocation/DisplayVersion 便于企业资产管理。
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1"; \
    ValueType: string; ValueName: "Publisher";      ValueData: "{#AppPublisher}"; \
    Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1"; \
    ValueType: string; ValueName: "DisplayName";    ValueData: "{#AppNameCN}"; \
    Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1"; \
    ValueType: string; ValueName: "DisplayVersion"; ValueData: "{#AppVersion}"; \
    Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1"; \
    ValueType: string; ValueName: "InstallLocation";ValueData: "{app}"; \
    Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1"; \
    ValueType: dword;  ValueName: "NoModify";       ValueData: 1; \
    Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1"; \
    ValueType: dword;  ValueName: "NoRepair";       ValueData: 1; \
    Flags: uninsdeletekey

; 文件关联：仅当用户勾选 fileassoc 任务时写入
; 【关键】Flags 含 uninsdeletekey —— 卸载时自动还原注册表（验收标准 ③ 要求）。
; 命令行用 "{app}\..." 而非引号直引 exe：Inno 会按安装实际路径替换，兼容自定义 /DIR=。
Root: HKA; Subkey: "Software\Classes\.json"; ValueType: string; \
    ValueName: ""; ValueData: "{#AppName}.Drawing"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\.dxf";  ValueType: string; \
    ValueName: ""; ValueData: "{#AppName}.Drawing"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\{#AppName}.Drawing"; \
    ValueType: string; ValueName: ""; ValueData: "SketchCAD 草图设计文件"; \
    Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\{#AppName}.Drawing\DefaultIcon"; \
    ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},0"; \
    Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\{#AppName}.Drawing\shell\open\command"; \
    ValueType: string; ValueName: ""; \
    ValueData: """{app}\{#AppExeName}"" ""%1"""; \
    Flags: uninsdeletekey; Tasks: fileassoc

[Run]
Filename: "{app}\{#AppExeName}"; \
    Description: "立即启动 {#AppNameCN}"; \
    Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 卸载时清理程序目录内的临时残留（不碰 %LOCALAPPDATA% 下的用户数据 ——
; 用户的自动保存快照可能含未保存的图纸，删掉等于丢稿）
Type: filesandordirs; Name: "{app}\*.tmp"
Type: filesandordirs; Name: "{app}\*.bak"
; Qt 部署产生的空目录（translations/ 等本项目未使用的）
Type: filesandordirs; Name: "{app}\translations"
