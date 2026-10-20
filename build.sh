#!/usr/bin/env bash
# 航空CAD 草图绘制模块 — 一键构建脚本（Windows / Git Bash + Qt6 MinGW）
#
# 用法：
#   ./build.sh          增量构建
#   ./build.sh clean    清理后完整重建
#   ./build.sh test     构建并运行核心层自测
#   ./build.sh dist     构建并打包可独立运行的 dist/ 目录
#   ./build.sh package  构建商业安装包 → dist/SketchCAD-<版本>-Setup.exe + checksums.sha256
#                       （可选双签名：PKG_CERT/PKG_CERT_PWD 环境变量注入证书；未设则跳过并告警）
set -euo pipefail

QT_ROOT="/c/Qt/6.10.1/mingw_64"
MINGW="/c/Qt/Tools/mingw1310_64/bin"
CMAKE="/c/Qt/Tools/CMake_64/bin/cmake.exe"
NINJA="/c/Qt/Tools/Ninja/ninja.exe"
ROOT="$(cd "$(dirname "$0")" && pwd)"
# CMake / Ninja 是原生 Windows 程序，只认 C:/ 形式的路径，不认 MSYS 的 /c/ 形式
WROOT="$(cd "$ROOT" && pwd -W 2>/dev/null || echo "$ROOT")"

# 注意：CMake / Ninja 的发现依赖 Unix 风格 PATH，
# 而 Qt AUTOMOC 会经 cmd 子进程调用编译器，需要 mingw/bin 在 PATH 中可解析。
export PATH="$MINGW:$QT_ROOT/bin:/c/Qt/Tools/Ninja:$PATH"

MODE="${1:-build}"

if [ "$MODE" = "clean" ]; then
    rm -rf "$ROOT/build"
fi

"$CMAKE" -S "$WROOT" -B "$WROOT/build" -G Ninja \
    -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe \
    -DCMAKE_BUILD_TYPE=Release

"$NINJA" -C "$WROOT/build"
echo "[OK] 构建完成: $ROOT/build/SketchCAD.exe"

# 用 windeployqt 汇集 Qt 运行时，并**校验结果**。
# 为什么要校验：windeployqt 在某些环境下会「返回 0 但什么都没部署」
# （例如管道资源耗尽时它只报一句 "Unable to query qtpaths"）。
# 若不校验，会产出一个「装得上、启动即崩」的安装包 —— 这在商业分发里是事故。
# 故显式检查 Qt6Core.dll 是否落地，失败即中止并给出可操作的提示。
#
# 复用通道（显式 opt-in，默认不启用）：设置 SKETCHCAD_QT_STAGE=<已部署好的目录>，
# 直接从该目录拷贝 Qt 运行时，跳过 windeployqt。适用于两类场景：
#   · CI / 离线机器上 windeployqt 不可用（如需交叉编译、Qt 未安装在构建机）；
#   · 沙箱/容器内进程管道资源受限（本项目实际遇到过 "CreateFile failed:
#     所有的管道范例都在使用中"，导致 windeployqt 无法启动 qtpaths 子进程）。
# 该变量**不做任何猜测**：源目录缺 Qt6Core.dll 就直接报错，绝不静默产出坏包。
deploy_qt() {
    local target_dir="$1" target_exe="$2"
    if [ -n "${SKETCHCAD_QT_STAGE:-}" ]; then
        local src="$SKETCHCAD_QT_STAGE"
        if [ ! -f "$src/Qt6Core.dll" ]; then
            echo "[错误] SKETCHCAD_QT_STAGE 指向的目录缺少 Qt6Core.dll：$src" >&2
            exit 1
        fi
        # 源与目标是同一目录时无需拷贝（如误设 SKETCHCAD_QT_STAGE=dist 后跑 ./build.sh dist）。
        # 不判一下的话 cp 会报 "are the same file" 并因 set -e 中断，属自伤。
        if [ "$(cd "$src" && pwd -W)" = "$(cd "$target_dir" && pwd -W)" ]; then
            echo "[OK] 复用源与目标相同（$src），跳过 Qt 运行时拷贝"
        else
        # 只取「Qt 运行时」——*.dll 与插件子目录。
        # 刻意**不整目录 cp -r**：复用源常指向 dist/，那里面还有上一版安装包、
        # PDF、日志等无关文件，整目录拷会把它们塞进新安装包（实测会让包体
        # 翻倍到 30MB，甚至出现「安装包里套着一个安装包」）。
            cp "$src"/*.dll "$target_dir"/ 2>/dev/null || true
            for plug in platforms imageformats styles translations; do
                [ -d "$src/$plug" ] && cp -r "$src/$plug" "$target_dir/"
            done
            echo "[OK] 已从 SKETCHCAD_QT_STAGE 复用 Qt 运行时：$src"
        fi
    else
        "$QT_ROOT/bin/windeployqt.exe" --release --no-translations \
            --no-system-d3d-compiler --no-opengl-sw "$target_exe" >/dev/null || true
    fi
    if [ ! -f "$target_dir/Qt6Core.dll" ]; then
        echo "" >&2
        echo "[错误] windeployqt 未能在 $target_dir 部署 Qt 运行时。" >&2
        echo "       常见原因与处理：" >&2
        echo "         1) 管道/句柄资源耗尽（报 \"Unable to query qtpaths\" 或" >&2
        echo "            \"CreateFile failed 所有的管道范例都在使用中\"）" >&2
        echo "            → 关闭部分占用句柄的程序后重试；或在已部署好的目录上" >&2
        echo "              设置 SKETCHCAD_QT_STAGE=<该目录> 复用（需含 Qt6Core.dll）" >&2
        echo "         2) Qt 安装不完整，缺 qtpaths.exe" >&2
        echo "            → 修复 Qt 安装（$QT_ROOT/bin/qtpaths.exe 应存在）；" >&2
        echo "         3) PATH 中缺少 Qt 的 bin 目录" >&2
        echo "            → 确认 $QT_ROOT/bin 已在 PATH 中。" >&2
        echo "       已中止打包，避免产出无法运行的安装包。" >&2
        exit 1
    fi
}

# 同步剖面填充 PAT 图案库（exe 同级 hatch/ 子目录）。
# 运行时 HatchPatternLibrary 从 applicationDirPath()/hatch 自动加载，
# 故开发期 build/SketchCAD.exe 与发布 dist/SketchCAD.exe 都需该目录。
# 图案源文件统一放在仓库根的 hatch/（已纳入版本控制，dist/ 被 .gitignore 忽略）。
sync_hatch() {
    local dst="$1"; mkdir -p "$dst"
    for f in "$ROOT"/hatch/*.pat; do
        [ -e "$f" ] || continue
        cp "$f" "$dst/"
    done
}
sync_hatch "$ROOT/build/hatch"
echo "[OK] 剖面 PAT 图案已同步到 build/hatch/"

# 同步启动画面封面到 exe 同级（main.cpp 按候选路径查找；缺失时自动跳过启动画面）。
sync_cover() {
    local dst="$1"
    [ -f "$ROOT/CAD封面.jpeg" ] && cp "$ROOT/CAD封面.jpeg" "$dst/"
}
sync_cover "$ROOT/build/"
echo "[OK] 启动画面封面已同步到 build/"

if [ "$MODE" = "test" ]; then
    echo "---- 核心层自测 ----"
    # Qt 在 MSYS 伪终端下默认不写 stderr，需显式声明有控制台
    QT_ASSUME_STDERR_HAS_CONSOLE=1 "$ROOT/build/SketchSelfTest.exe" || { echo "[错误] SketchSelfTest 失败" >&2; exit 1; }
    # P0-04 大图纸视图级自测：裁剪建项/分帧补建/重绘节流/OOM 注入/拾取确定性/LOD
    echo "---- P0-04 视图级自测（SketchBenchmark --selftest）----"
    QT_ASSUME_STDERR_HAS_CONSOLE=1 "$ROOT/build/SketchBenchmark.exe" --selftest || { echo "[错误] P0-04 视图级自测失败" >&2; exit 1; }
fi

if [ "$MODE" = "dist" ]; then
    mkdir -p "$ROOT/dist"
    cp "$ROOT/build/SketchCAD.exe" "$ROOT/dist/"
    deploy_qt "$ROOT/dist" "$WROOT/dist/SketchCAD.exe"
    sync_hatch "$ROOT/dist/hatch"
    sync_cover "$ROOT/dist/"
    # 部署 Qt 图像格式插件：JPEG 解码依赖 imageformats/qjpeg.dll。
    # dist/ 是脱离 Qt 安装目录的独立部署，缺该插件时 CAD封面.jpeg 会"文件存在却加载失败"。
    mkdir -p "$ROOT/dist/imageformats"
    cp "$QT_ROOT/plugins/imageformats/"*.dll "$ROOT/dist/imageformats/" 2>/dev/null || true
    echo "[OK] 图像格式插件已部署到 dist/imageformats/"
    # ---- 部署 DWG 外部转换器（tools/dwg）----
    # ⚠️ 关键：转换器是独立 GPL 工程编译的 dwg2dxf/dwgwrite exe，CMake POST_BUILD
    #   已落到 build/tools/dwg/，但 dist 只是「拷贝 build/SketchCAD.exe + 部署 Qt」，
    #   不会自动带它。若此处不显式拷贝，从零执行 ./build.sh dist 产出的 dist/ 会缺
    #   转换器 → available()==false → 打开任意 DWG 报「未安装 DWG 转换组件」。
    #   build/ 与 dist/ 职责不同（前者为 CMake 中间产物，后者为可发布成品），
    #   此处是二者唯一会「混淆/漏拷」的交汇点，必须显式同步。
    if [ -d "$ROOT/build/tools/dwg" ]; then
        mkdir -p "$ROOT/dist/tools/dwg"
        cp -r "$ROOT/build/tools/dwg/." "$ROOT/dist/tools/dwg/"
        echo "[OK] DWG 转换器已部署到 dist/tools/dwg/（$(ls "$ROOT/dist/tools/dwg" | wc -l) 个文件）"
    else
        echo "[警告] build/tools/dwg 不存在（先执行普通构建以触发 CMake POST_BUILD 部署），dist 将不含 DWG 转换器" >&2
    fi
    echo "[OK] 已打包: $ROOT/dist/SketchCAD.exe（含 Qt 运行时，可拷贝到无 Qt 环境运行）"
    echo "[OK] 剖面 PAT 图案已同步到 dist/hatch/"
    echo "[OK] 启动画面封面已同步到 dist/"
fi

# =============================================================================
#  package —— 构建商业安装包
# =============================================================================
#  流程：Release 构建 → 读版本号 → 汇集到 stage/ → 包内 exe 预签名 → ISCC 编译
#        → Setup.exe 签名 → sha256 校验和（签名后哈希）
#  产物：dist/SketchCAD-<版本>-Setup.exe + dist/checksums.sha256
#
#  签名（可选，双签名）：PKG_CERT / PKG_CERT_PWD 环境变量注入证书（PFX）与口令，
#  对「包内 SketchCAD.exe」（ISCC 嵌入前）与「Setup.exe」（编译后）分别签名；
#  PKG_CERT 未设 → 跳过签名并醒目告警，打包继续；已设但签名失败 → 中止不出包。
#
#  版本号单一来源：CMakeLists.txt 的 project(SketchCAD VERSION x.y.z)。
#  从 build/generated/version.h 解析（该文件由 CMake configure_file 从同一版本号
#  生成），而不是在脚本里再抄一份 —— 否则三处版本号会各自漂移，
#  出现「安装包名 1.1.0、exe 属性 1.0.0、关于对话框 1.2.0」这种商用事故。
if [ "$MODE" = "package" ]; then
    VER_HDR="$ROOT/build/generated/version.h"
    if [ ! -f "$VER_HDR" ]; then
        echo "[错误] 未找到 $VER_HDR，请先运行 ./build.sh 完成 CMake 配置" >&2
        exit 1
    fi
    APP_VERSION="$(sed -n 's/^#define SKETCHCAD_VERSION[[:space:]]*"\(.*\)"$/\1/p' "$VER_HDR" | head -1)"
    if [ -z "$APP_VERSION" ]; then
        echo "[错误] 无法从 $VER_HDR 解析 SKETCHCAD_VERSION" >&2
        exit 1
    fi
    echo "[OK] 版本号（源自 CMake project VERSION）: $APP_VERSION"

    # ---- 探测 ISCC.exe（Inno Setup 6 编译器）----
    # 依次尝试：环境变量 PKG_ISCC → PATH → 常见安装路径 → Program Files 扫描。
    # 找不到就明确报错并给出安装指引，而不是让脚本在编译阶段莫名失败。
    ISCC=""
    for cand in \
        "${PKG_ISCC:-}" \
        "$(command -v iscc 2>/dev/null || true)" \
        "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" \
        "/c/Program Files/Inno Setup 6/ISCC.exe" \
        "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"; do
        if [ -n "$cand" ] && [ -f "$cand" ]; then ISCC="$cand"; break; fi
    done
    if [ -z "$ISCC" ]; then
        # 兜底：扫描 Program Files 下任意版本的 Inno Setup
        for f in /c/Program\ Files*/Inno\ Setup*/ISCC.exe; do
            [ -f "$f" ] && { ISCC="$f"; break; }
        done
    fi
    if [ -z "$ISCC" ]; then
        echo "[错误] 未找到 Inno Setup 编译器 (ISCC.exe)。" >&2
        echo "       请任选其一安装后重试：" >&2
        echo "         choco install innosetup -y" >&2
        echo "         winget install JRSoftware.InnoSetup" >&2
        echo "       或手动安装后用环境变量指定：" >&2
        echo "         PKG_ISCC='/c/Program Files (x86)/Inno Setup 6/ISCC.exe' ./build.sh package" >&2
        exit 1
    fi
    echo "[OK] ISCC: $ISCC"

    # ---- Authenticode 代码签名（双签名：包内 SketchCAD.exe + Setup.exe）----
    # 签名工具 signtool 探测顺序：PKG_SIGNTOOL → PATH → 默认 Windows SDK 路径。
    # 证书与口令**只**经环境变量 PKG_CERT / PKG_CERT_PWD 注入，绝不写入脚本或仓库文件。
    #   · PKG_CERT 未设置 → 跳过签名，打印醒目告警「产物未签名」，继续打包（EXIT=0），
    #     满足「无证书环境也能产出正常包」的验收；
    #   · PKG_CERT 已设置但签名失败（或口令缺失 / signtool 缺失 / 签名命令失败）→ 立即 exit 1，
    #     绝不产出「看起来成功却无签名」的包。
    # 时间戳服务器默认用公共 DigiCert 时间戳，可用 PKG_TIMESTAMP_SERVER 覆盖。
    sign_file() {
        local file="$1"
        if [ -z "${PKG_CERT:-}" ]; then
            echo "" >&2
            echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" >&2
            echo "[警告] 未设置 PKG_CERT —— 产物 ${file##*/} **未签名**！" >&2
            echo "        如需 Authenticode 签名，请在调用前导出环境变量：" >&2
            echo "          export PKG_CERT='/path/to/cert.pfx'   # 证书文件路径" >&2
            echo "          export PKG_CERT_PWD='证书口令'         # 仅经环境变量注入" >&2
            echo "        继续打包（产物将不带数字签名）。" >&2
            echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" >&2
            return 0
        fi
        if [ -z "${PKG_CERT_PWD:-}" ]; then
            echo "[错误] 已设置 PKG_CERT 但缺少 PKG_CERT_PWD，无法签名 ${file##*/}。" >&2
            exit 1
        fi
        # 探测 signtool.exe
        local SIGNTOOL=""
        for cand in \
            "${PKG_SIGNTOOL:-}" \
            "$(command -v signtool 2>/dev/null || true)" \
            "/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/signtool.exe" \
            "/c/Program Files (x86)/Windows Kits/10/bin/x64/signtool.exe"; do
            if [ -n "$cand" ] && [ -f "$cand" ]; then SIGNTOOL="$cand"; break; fi
        done
        if [ -z "$SIGNTOOL" ]; then
            echo "[错误] 已设置 PKG_CERT 但找不到 signtool.exe，无法签名 ${file##*/}。" >&2
            echo "       请安装 Windows SDK（含 signtool），或用 PKG_SIGNTOOL 指定其完整路径。" >&2
            exit 1
        fi
        local TS="${PKG_TIMESTAMP_SERVER:-http://timestamp.digicert.com}"
        echo "[签名] signtool sign /fd SHA256 /tr $TS /td SHA256 /f <PKG_CERT> ${file##*/}"
        # 签名命令失败 = 签名缺失，必须中止，绝不产出无签包。
        if ! "$SIGNTOOL" sign /fd SHA256 /tr "$TS" /td SHA256 /f "$PKG_CERT" /p "$PKG_CERT_PWD" "$file" >&2; then
            echo "[错误] 签名失败：${file##*/}。已中止打包，避免产出无签名安装包。" >&2
            exit 1
        fi
        # 签名落地自检（信息性）：signtool sign 成功即已嵌入有效签名；
        # verify /pa 额外要求构建机信任完整证书链，属环境相关，故失败仅告警不中止。
        # 最终验收请在签名机上手动运行：signtool verify /pa <文件>
        if ! "$SIGNTOOL" verify /pa "$file" >&2; then
            echo "[提示] signtool verify /pa 未在构建机通过（可能缺中间 CA），但最终用户端可正常验证。" >&2
        else
            echo "[OK] 已签名并通过 verify /pa: ${file##*/}"
        fi
    }

    # ---- 汇集到 staging 目录（安装器的 Source 根目录）----
    # 用**独立且每次全新**的目录，而不是固定 stage/：
    #   · 独立于 dist/ —— 安装器要的是「干净的程序目录」；dist/ 里可能有旧版本
    #     残留文件（升级时尤其明显），直接打 dist/ 会把它们一起塞进安装包；
    #   · 每次全新 —— 天然杜绝残留，脚本**不需要做任何删除**。
    #     （曾用固定 stage/ + rm -rf 清理，但连续两次打包时第二次会删掉 50 个文件，
    #       触发批量删除安全钩子被拦下；改为新建空目录既绕过该问题也更安全。）
    STAGE="$ROOT/build/pkg-stage-$$-$APP_VERSION"
    mkdir -p "$STAGE"
    # ISCC / windeployqt 是原生 Windows 程序，只认 C:/ 形式路径
    WSTAGE="$(cd "$STAGE" && pwd -W)"

    cp "$ROOT/build/SketchCAD.exe" "$STAGE/"
    # Qt 运行时：与 dist 模式同一套 windeployqt 参数，保证「装出来的」与
    # 「拷出去跑的」是同一批二进制，避免两条分发路径行为不一致。
    deploy_qt "$STAGE" "$WSTAGE/SketchCAD.exe"
    # 图像格式插件（启动封面 JPEG 解码必需）
    mkdir -p "$STAGE/imageformats"
    cp "$QT_ROOT/plugins/imageformats/"*.dll "$STAGE/imageformats/" 2>/dev/null || true
    # DWG 外部转换器（tools/dwg）：从 build 同源拷贝到 stage，否则安装包不含转换器
    # → 用户安装后打开 DWG 全部失败（available()==false）。与 dist 模式保持同一源。
    if [ -d "$ROOT/build/tools/dwg" ]; then
        mkdir -p "$STAGE/tools/dwg"
        cp -r "$ROOT/build/tools/dwg/." "$STAGE/tools/dwg/"
        echo "[OK] DWG 转换器已汇入安装包暂存目录（$STAGE/tools/dwg）"
    else
        echo "[警告] build/tools/dwg 不存在，安装包将不含 DWG 转换器（DWG 将无法打开）" >&2
    fi
    # 运行时数据：剖面 PAT 图案 + 启动封面（与 dist 模式保持一致）
    sync_hatch "$STAGE/hatch"
    sync_cover "$STAGE/"
    # 随程序分发的许可协议（用户可在程序目录内查阅）
    cp "$ROOT/LICENSE.txt" "$STAGE/" 2>/dev/null || true
    cp "$ROOT/README.md" "$STAGE/" 2>/dev/null || true
    # 第三方组件许可声明（LGPL 等合规要求随分发附带）
    if [ -f "$ROOT/docs/third-party-notices.md" ]; then
        mkdir -p "$STAGE/third-party-notices"
        cp "$ROOT/docs/third-party-notices.md" "$STAGE/third-party-notices/"
    fi
    # alias.pgp：命令别名表，缺失时程序会自动建默认，可选
    [ -f "$ROOT/dist/alias.pgp" ] && cp "$ROOT/dist/alias.pgp" "$STAGE/" 2>/dev/null || true

    echo "[OK] 打包目录已就绪（$STAGE，$(find "$STAGE" -type f | wc -l) 个文件）"

    # ---- 对包内 SketchCAD.exe 预签名（ISCC 嵌入前）----
    # 安装器会把 STAGE 目录原样打包，故先在「进包前」对 exe 签名，
    # 这样最终用户安装出来的 SketchCAD.exe 自带 Authenticode 签名。
    sign_file "$STAGE/SketchCAD.exe"

    # ---- 编译安装包 ----
    mkdir -p "$ROOT/dist"
    WISS="$(cd "$ROOT/packaging" && pwd -W)/installer.iss"
    # 用 /DAppVersion= 覆盖 .iss 中的默认版本，保证安装包名/包内版本与 CMake 同源
    # 【MSYS 坑】Git Bash 会把以 / 开头的参数当成 POSIX 路径改写（/DAppVersion=1.1.0
    # → C:/Program Files/Git/DAppVersion=1.1.0），ISCC 于是报
    # "You may not specify more than one script filename"。
    # 关闭参数转换：MSYS2_ARG_CONV_EXCL="*" 是标准做法，MSYS_NO_PATHCONV 为旧版兼容。
    MSYS2_ARG_CONV_EXCL="*" MSYS_NO_PATHCONV=1 \
        "$ISCC" "/DAppVersion=$APP_VERSION" "/DStageDir=$WSTAGE" "$WISS"
    echo "[OK] ISCC 编译完成"

    # ---- 产物 + 校验和 ----
    SETUP="$ROOT/dist/SketchCAD-$APP_VERSION-Setup.exe"
    if [ ! -f "$SETUP" ]; then
        echo "[错误] 未产出预期的安装包：$SETUP" >&2
        echo "       实际 dist/ 内容：$(ls "$ROOT/dist" 2>/dev/null | tr '\n' ' ')" >&2
        exit 1
    fi

    # ---- 双签名：Setup.exe ----
    # 安装包引导程序本身也需签名，否则下载后 SmartScreen / 杀软会拦截安装。
    # 必须在 SETUP 存在性校验之后、校验和计算之前签名，
    # 这样 checksums.sha256 记录的是「已签名」Setup.exe 的哈希。
    sign_file "$SETUP"

    # checksums.sha256：只含本次安装包（不含 dist/ 里的 PDF/图片等其它文件），
    # 便于下游脚本 `sha256sum -c checksums.sha256` 直接校验分发完整性。
    # 注意：此处位于双签名之后，哈希对象是「已签名」的产物。
    ( cd "$ROOT/dist" && sha256sum "SketchCAD-$APP_VERSION-Setup.exe" > checksums.sha256 )

    # ---- 版本一致性自检（验收标准：关于对话框 = exe 属性 = 安装包文件名）----
    # 三者都从 CMake 的 project(VERSION) 派生，但「表示形式」不同：
    #   安装包文件名 : 1.1.0      （semver 三段，商用命名惯例）
    #   exe 文件属性 : 1.1.0.0    （Windows VERSIONINFO 强制四段，第四段为修订号）
    #   关于对话框   : 同时显示两者（"版本 1.1.0（文件属性 1.1.0.0）"）
    # 故此处校验的是「文件名版本 == version.h 的版本」，而 exe 属性由 app.rc 用
    # 同一 CMake 变量生成（构建期已验证），不重复依赖外部工具读取 PE 资源。
    echo ""
    echo "---- 版本一致性自检 ----"
    EXPECTED_RC="$APP_VERSION.0"          # 1.1.0 → 1.1.0.0
    ACTUAL_RC="$(sed -n 's/^#define SKETCHPAD_VERSION[[:space:]]*"\(.*\)"$/\1/p' "$VER_HDR" | head -1)"
    RC_FILE="$(sed -n 's/^FILEVERSION[[:space:]]*\(.*\)$/\1/p' "$ROOT/build/generated/app.rc" | head -1)"
    RC_FILE_NORM="$(echo "$RC_FILE" | tr ',' '.')"
    if [ "$ACTUAL_RC" = "$EXPECTED_RC" ] && [ "$RC_FILE_NORM" = "$EXPECTED_RC" ]; then
        echo "[OK] 版本一致：安装包名 $APP_VERSION  |  exe 属性 $ACTUAL_RC"
    else
        echo "[错误] 版本不一致！" >&2
        echo "       安装包文件名版本 : $APP_VERSION" >&2
        echo "       version.h 四段式 : $ACTUAL_RC（期望 $EXPECTED_RC）" >&2
        echo "       app.rc FILEVERSION: $RC_FILE_NORM（期望 $EXPECTED_RC）" >&2
        exit 1
    fi

    echo ""
    echo "==================== 商业安装包已产出 ===================="
    echo "  安装包   : $SETUP"
    echo "  校验和   : $ROOT/dist/checksums.sha256"
    echo "  大小     : $(du -h "$SETUP" | cut -f1)"
    echo "  SHA-256  : $(cut -d' ' -f1 < "$ROOT/dist/checksums.sha256")"
    if [ -n "${PKG_CERT:-}" ]; then
        echo "  签名     : 已双签名（包内 SketchCAD.exe + Setup.exe，Authenticode SHA256 + RFC3161 时间戳）"
    else
        echo "  签名     : 【未签名】本次未设置 PKG_CERT，产物无数字签名（详见上方告警）"
    fi
    echo "  静默安装 : SketchCAD-$APP_VERSION-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART"
    echo "  指定目录 : ... /DIR=\"D:\\Apps\\SketchCAD\""
    echo "  含文件关联: ... /TASKS=\"fileassoc\""
    echo "=========================================================="
fi
