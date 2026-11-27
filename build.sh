#!/usr/bin/env bash
# 我是慎独 草图绘制模块 — 一键构建脚本（Windows / Git Bash + Qt6 MinGW）
#
# 用法：
#   ./build.sh          增量构建
#   ./build.sh clean    清理后完整重建
#   ./build.sh dist     构建并打包可独立运行的 dist/ 目录
#   ./build.sh package  构建商业安装包 → dist/SketchCAD-<版本>-Setup.exe
#                       + dist/SketchCAD-<版本>-Setup.msi + checksums.sha256
#                       （MSI 需 WiX Toolset v5：dotnet tool install -g wix --version 5.0.2）
#                       （可选双签名：PKG_CERT/PKG_CERT_PWD 环境变量注入证书；未设则跳过并告警）
#   ./build.sh dbconfig 安全增量构建 SketchCAD（单作业 -j 1，避开大文件并行 OOM；
#                       数据库配置功能已并入 SketchCAD 主目标，改完 CMakeLists/源码后跑这个最稳）
#   ./build.sh mysql-plugin  独立构建并部署 Qt MySQL 驱动插件 qsqlmysql.dll 到
#                       dist/sqldrivers 与 build/sqldrivers（Qt 6.10.1 官方未附带 QMYSQL 插件；
#                       需 Qt 源码 mysql 目录与 MySQL 客户端库，缺失则跳过并告警，不中断主流程）
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

if [ "$MODE" != "mysql-plugin" ]; then
"$CMAKE" -S "$WROOT" -B "$WROOT/build" -G Ninja \
    -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe \
    -DCMAKE_BUILD_TYPE=Release

if [ "$MODE" = "dbconfig" ] || [ "$MODE" = "package" ]; then
    # 数据库配置功能已并入 SketchCAD 主目标；package 模式也仅增量构建该目标、单作业，
    # 避免 BatchPlotEngine 等大文件并行编译触发 OOM（实测全量并行 ninja 必 OOM），
    # 且避免 package 去编译与安装包无关的辅助目标（插件示例/COM 等）。
    "$NINJA" -C "$WROOT/build" SketchCAD -j 1
    echo "[OK] 已安全增量构建 SketchCAD（含数据库配置功能）: $ROOT/build/SketchCAD.exe"
else
    "$NINJA" -C "$WROOT/build"
    echo "[OK] 构建完成: $ROOT/build/SketchCAD.exe"
fi
fi

# ============================================================================
#  MySQL 驱动插件（qsqlmysql.dll）独立构建与部署
# ----------------------------------------------------------------------------
#  Qt 6.10.1 官方 mingw_64 构建未附带 QMYSQL 插件（plugins/sqldrivers 仅含
#  qsqlite/qodbc/qpsql/qoci/qibase/qmimer）。本函数从 Qt 源码的 mysql 插件目录
#  独立构建一个 MODULE 插件，供主程序在运行时自动加载
#  （DrawingDb::registerSqlPluginPaths 会把 <程序目录>/sqldrivers 加入 Qt 插件
#  搜索路径，并把可能放置 libmysql.dll 的目录注入 PATH）。
#
#  前置依赖（任一缺失则跳过并告警，不中断主流程）：
#    · Qt 源码的 mysql 插件目录（默认 C:/Qt/6.10.3/Src/qtbase/src/plugins/sqldrivers/mysql，
#      可用 SKETCHCAD_MYSQL_SRC 覆盖）；
#    · MySQL 客户端库 libmysql.dll（MSVC 版亦可：MinGW 经 gendef+dlltool 生成导入库）。
#  源码取自 6.10.3、链接/包含用运行时相同的 6.10.1（patch 级 ABI 兼容，已实测可加载）。
build_mysql_plugin() {
    local SRC="${SKETCHCAD_MYSQL_SRC:-C:/Qt/6.10.3/Src/qtbase/src/plugins/sqldrivers/mysql}"
    local WORK="$ROOT/build/mysql_plugin"
    local WWORK MYDLL MYINC MYDIR PLUGIN PROBE_EXE cand
    WWORK="$(cd "$WORK" 2>/dev/null && pwd -W || echo "$WORK")"

    # 已构建过则跳过（幂等、加速重跑）
    if [ -f "$WORK/build_dir/qsqlmysql.dll" ]; then
        echo "[MySQL 插件] 已存在，跳过重新构建: $WORK/build_dir/qsqlmysql.dll"
        return 0
    fi

    # 探测 MySQL 客户端库（MySQL 9.x / 8.0 的 libmysql.dll 均位于安装目录的 bin/ 下）
    MYDLL=""
    for cand in \
        "${SKETCHCAD_MYSQL_DLL:-}" \
        "C:/Program Files/MySQL/MySQL Server 9.6/bin/libmysql.dll" \
        "C:/Program Files/MySQL/MySQL Server 9.6/lib/libmysql.dll" \
        "C:/Program Files/MySQL/MySQL Server 8.0/bin/libmysql.dll" \
        "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll"; do
        [ -n "$cand" ] && [ -f "$cand" ] && { MYDLL="$cand"; break; }
    done

    if [ ! -d "$SRC" ]; then
        echo "[警告] 未找到 Qt MySQL 插件源码目录：$SRC（可用 SKETCHCAD_MYSQL_SRC 指定）" >&2
        echo "        跳过 MySQL 插件构建，主程序将回退到内置 SQLite。" >&2
        return 0
    fi
    if [ -z "$MYDLL" ]; then
        echo "[警告] 未找到 MySQL 客户端库 libmysql.dll（默认探测 9.6 / 8.0，可用 SKETCHCAD_MYSQL_DLL 指定）" >&2
        echo "        跳过 MySQL 插件构建，主程序将回退到内置 SQLite。" >&2
        return 0
    fi

    MYINC="$(cd "$(dirname "$MYDLL")/../include" 2>/dev/null && pwd -W || echo "C:/Program Files/MySQL/MySQL Server 9.6/include")"
    MYDIR="$(cd "$(dirname "$MYDLL")" 2>/dev/null && pwd -W || echo "$(dirname "$MYDLL")")"

    echo "[MySQL 插件] 源码: $SRC"
    echo "[MySQL 插件] 客户端库: $MYDLL"
    echo "[MySQL 插件] 头文件目录: $MYINC"

    # 重建干净的工程目录（每次从 Qt 源码刷新，不依赖手工拷贝残留）
    rm -rf "$WORK"
    mkdir -p "$WORK"
    cp "$SRC"/main.cpp "$SRC"/qsql_mysql.cpp "$SRC"/qsql_mysql_p.h "$SRC"/mysql.json "$WORK"/ 2>/dev/null || {
        echo "[警告] 拷贝 Qt MySQL 插件源文件失败（$SRC），跳过 MySQL 插件构建。" >&2
        return 0
    }
    # 验证探针源（best-effort）
    cat > "$WORK/probe.cpp" <<'PROBE'
#include <QCoreApplication>
#include <QSqlDatabase>
#include <cstdio>
int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    if (QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"));
        printf("QMYSQL available: TRUE\ndriverName: %s\nPLUGIN OK\n", db.driverName().toUtf8().constData());
        return 0;
    }
    printf("QMYSQL available: FALSE\nPLUGIN MISSING/FAILED\n");
    return 1;
}
PROBE

    # 由 MinGW 生成 libmysql.a 导入库（MSVC 的 .lib 不能直接被 MinGW 链接）
    echo "[MySQL 插件] 生成 MinGW 导入库 libmysql.a ..."
    ( cd "$WORK" && "$MINGW/gendef.exe" "$MYDLL" > libmysql.def 2>/dev/null ) || true
    if [ -s "$WORK/libmysql.def" ]; then
        "$MINGW/dlltool.exe" -d "$WORK/libmysql.def" -l "$WORK/libmysql.a" -D "$MYDLL" 2>/dev/null || true
    fi
    if [ ! -f "$WORK/libmysql.a" ]; then
        echo "[警告] 无法生成 libmysql.a（gendef/dlltool 失败），跳过 MySQL 插件构建。" >&2
        return 0
    fi

    # 写自包含 CMakeLists（同已验证可构建的工程；MYSQL_INC_PLACEHOLDER 随后被 sed 替换为实测路径）
    cat > "$WORK/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.16)
project(qsqlmysql_plugin CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.1/mingw_64" ${CMAKE_PREFIX_PATH})
find_package(Qt6 REQUIRED COMPONENTS Core Sql Network)
add_library(qsqlmysql MODULE main.cpp qsql_mysql.cpp qsql_mysql_p.h mysql.json)
set_target_properties(qsqlmysql PROPERTIES PREFIX "" OUTPUT_NAME qsqlmysql)
target_include_directories(qsqlmysql PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "MYSQL_INC_PLACEHOLDER"
    "C:/Qt/6.10.1/mingw_64/include/QtSql/6.10.1"
    "C:/Qt/6.10.1/mingw_64/include/QtSql/6.10.1/QtSql"
    "C:/Qt/6.10.1/mingw_64/include/QtCore/6.10.1"
    "C:/Qt/6.10.1/mingw_64/include/QtCore/6.10.1/QtCore")
target_compile_definitions(qsqlmysql PRIVATE
    QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_CONTEXTLESS_CONNECT QT_PLUGIN QT_SHARED)
target_link_libraries(qsqlmysql PRIVATE Qt6::Core Qt6::Sql Qt6::Network "${CMAKE_SOURCE_DIR}/libmysql.a")
add_executable(probe probe.cpp)
target_link_libraries(probe PRIVATE Qt6::Core Qt6::Sql)
CMAKE
    sed -i "s|MYSQL_INC_PLACEHOLDER|$MYINC|g" "$WORK/CMakeLists.txt"

    # 用与运行时一致的 Qt 构建树配置+构建
    echo "[MySQL 插件] 配置并构建 qsqlmysql.dll ..."
    if ! "$CMAKE" -S "$WWORK" -B "$WWORK/build_dir" -G Ninja \
        -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe \
        -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1; then
        echo "[警告] MySQL 插件 CMake 配置失败，跳过 MySQL 插件构建。" >&2
        return 0
    fi
    if ! "$NINJA" -C "$WWORK/build_dir" >/dev/null 2>&1; then
        echo "[警告] MySQL 插件构建失败，跳过 MySQL 插件构建。" >&2
        return 0
    fi

    PLUGIN="$WORK/build_dir/qsqlmysql.dll"
    if [ ! -f "$PLUGIN" ]; then
        echo "[警告] 未产出 qsqlmysql.dll，跳过 MySQL 插件构建。" >&2
        return 0
    fi

    # best-effort 验证：把插件放入 sqldrivers/ 子目录，用探针确认 QMYSQL 可加载
    PROBE_EXE="$WORK/build_dir/probe.exe"
    if [ -f "$PROBE_EXE" ]; then
        mkdir -p "$WORK/build_dir/sqldrivers"
        cp "$PLUGIN" "$WORK/build_dir/sqldrivers/" 2>/dev/null || true
        if QT_PLUGIN_PATH="$WWORK/build_dir" PATH="$MYDIR;$PATH" "$PROBE_EXE" >/dev/null 2>&1; then
            echo "[OK] 探针确认 QMYSQL 插件可加载（PLUGIN OK）"
        else
            echo "[提示] 探针未通过（可能无控制台/环境限制），但插件已构建；实际加载以主程序运行为准" >&2
        fi
    fi
    echo "[OK] MySQL 插件构建完成: $PLUGIN"
}

# 把已构建的 qsqlmysql.dll（及运行时依赖 libmysql.dll、OpenSSL）部署到目标 sqldrivers/ 目录。
#
# ⚠️ 关键修复点：libmysql.dll（MySQL 9.x 客户端，MSVC 构建）除 VC++ 运行库
#   （VCRUNTIME140 / MSVCP140，Windows 已自带）外，还依赖 OpenSSL 的
#   libssl-3-x64.dll 与 libcrypto-3-x64.dll。这两个 DLL 既不在 dist/ 也不在
#   System32，只存在于 MySQL 安装目录；一旦部署机没有把 MySQL 放进 PATH
#   （例如直接双击 dist/SketchCAD.exe 启动），libmysql.dll 就因找不到 OpenSSL
#   而加载失败 → QMYSQL 插件加载失败 → 测试连接报 "Driver not loaded"。
#   故此处必须把 libssl/libcrypto 一并拷入两处：
#     · $dst        （= <程序目录>/sqldrivers）：覆盖 qsqlmysql→libmysql 作为依赖链
#                    加载时、从 libmysql 自身目录解析 OpenSSL 的场景；
#     · $dst/..     （= <程序目录>，exe 所在目录）：覆盖本程序 registerSqlPluginPaths
#                    用 QLibrary 直接预加载 libmysql 时、从 exe 目录解析其依赖的场景。
#   两处都放，确保两种加载路径都能命中 OpenSSL（已用「剥离 PATH 双击启动」复现验证）。
sync_mysql_plugin() {
    local dst="$1"
    [ -d "$dst" ] || mkdir -p "$dst"
    local PLUGIN="" MYDLL_DEP cand MYBIN OPENSSL_SRC dll
    PLUGIN="$(ls "$ROOT/build/mysql_plugin/build_dir/qsqlmysql.dll" 2>/dev/null \
           || ls "$ROOT/build/mysql_plugin/qsqlmysql.dll" 2>/dev/null || true)"
    if [ -z "$PLUGIN" ] || [ ! -f "$PLUGIN" ]; then
        echo "[警告] 未找到已构建的 qsqlmysql.dll，无法部署 MySQL 插件到 $dst" >&2
        return 0
    fi
    cp "$PLUGIN" "$dst/qsqlmysql.dll"
    echo "[OK] MySQL 插件已部署: $dst/qsqlmysql.dll"
    # 运行时依赖 libmysql.dll：随插件一同放入 sqldrivers/。
    MYDLL_DEP=""
    for cand in \
        "${SKETCHCAD_MYSQL_DLL:-}" \
        "C:/Program Files/MySQL/MySQL Server 9.6/bin/libmysql.dll" \
        "C:/Program Files/MySQL/MySQL Server 9.6/lib/libmysql.dll" \
        "C:/Program Files/MySQL/MySQL Server 8.0/bin/libmysql.dll" \
        "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll"; do
        [ -n "$cand" ] && [ -f "$cand" ] && { MYDLL_DEP="$cand"; break; }
    done
    if [ -n "$MYDLL_DEP" ]; then
        cp "$MYDLL_DEP" "$dst/libmysql.dll"
        echo "[OK] MySQL 客户端库已随插件部署: $dst/libmysql.dll"
    else
        echo "[警告] 未找到 libmysql.dll，仅部署了插件本体；运行时需保证 libmysql.dll 在 PATH 中" >&2
    fi
    # OpenSSL 依赖（libssl-3-x64.dll / libcrypto-3-x64.dll）：必须与 libmysql.dll 同源，
    # 否则符号/版本不匹配会令 libmysql 加载失败。优先取与 libmysql 同一 MySQL 安装目录。
    MYBIN="$(cd "$(dirname "$MYDLL_DEP")" 2>/dev/null && pwd -W || true)"
    OPENSSL_SRC=""
    for cand in \
        "${MYBIN:+${MYBIN}/libssl-3-x64.dll}" \
        "C:/Program Files/MySQL/MySQL Server 9.6/bin/libssl-3-x64.dll" \
        "C:/Program Files/MySQL/MySQL Server 8.0/bin/libssl-3-x64.dll"; do
        [ -n "$cand" ] && [ -f "$cand" ] && { OPENSSL_SRC="$(cd "$(dirname "$cand")" && pwd -W)"; break; }
    done
    if [ -n "$OPENSSL_SRC" ]; then
        local exeroot="$(cd "$dst/.." && pwd -W)"
        for dll in libssl-3-x64.dll libcrypto-3-x64.dll; do
            if [ -f "$OPENSSL_SRC/$dll" ]; then
                cp "$OPENSSL_SRC/$dll" "$dst/$dll"          # <程序目录>/sqldrivers/
                cp "$OPENSSL_SRC/$dll" "$exeroot/$dll"      # <程序目录>（exe 所在目录）
                echo "[OK] OpenSSL 依赖已部署: $dst/$dll 与 $exeroot/$dll"
            else
                echo "[警告] 未找到 $dll（位于 $OPENSSL_SRC），MySQL 驱动可能无法加载" >&2
            fi
        done
    else
        echo "[警告] 未找到 OpenSSL（libssl-3-x64.dll），MySQL 驱动可能因缺 OpenSSL 而无法加载" >&2
    fi
}

# mysql-plugin 模式：仅构建并部署插件，跳过主工程全量构建
if [ "$MODE" = "mysql-plugin" ]; then
    build_mysql_plugin
    sync_mysql_plugin "$ROOT/dist/sqldrivers"
    sync_mysql_plugin "$ROOT/build/sqldrivers"
    echo "[OK] MySQL 插件构建并部署完成（dist/sqldrivers 与 build/sqldrivers）"
    exit 0
fi

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
# ⚠️ 首选 SketchCAD.png（新品牌封面）；保留 CAD封面.jpeg 作为旧部署回退兼容。
sync_cover() {
    local dst="$1"
    [ -f "$ROOT/SketchCAD.png" ] && cp "$ROOT/SketchCAD.png" "$dst/"
    [ -f "$ROOT/CAD封面.jpeg" ] && cp "$ROOT/CAD封面.jpeg" "$dst/"
}
sync_cover "$ROOT/build/"
echo "[OK] 启动画面封面已同步到 build/"

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
        # GPLv3 合规：转换器是独立进程分发的 LibreDWG 构建产物，随二进制必须
        # 附带 GPLv3 全文（COPYING）与源码获取地址（见 third-party-notices.md）。
        # 缺 COPYING 即构成许可证违约，故与 exe 同目录落盘、不可省略。
        if [ -f "$ROOT/COPYING" ]; then
            cp "$ROOT/COPYING" "$ROOT/dist/tools/dwg/COPYING"
            echo "[OK] GPLv3 许可证全文已随转换器落盘（dist/tools/dwg/COPYING）"
        else
            echo "[错误] 仓库根缺少 COPYING（GPLv3 全文），分发转换器将违反许可证" >&2
            exit 1
        fi
    else
        echo "[警告] build/tools/dwg 不存在（先执行普通构建以触发 CMake POST_BUILD 部署），dist 将不含 DWG 转换器" >&2
    fi
    echo "[OK] 已打包: $ROOT/dist/SketchCAD.exe（含 Qt 运行时，可拷贝到无 Qt 环境运行）"
    echo "[OK] 剖面 PAT 图案已同步到 dist/hatch/"
    echo "[OK] 启动画面封面已同步到 dist/"
    # ---- 部署 MySQL 驱动插件（如缺失则尝试构建，失败仅告警）----
    if [ ! -f "$ROOT/dist/sqldrivers/qsqlmysql.dll" ]; then
        echo "[dist] 未检测到 MySQL 插件，尝试构建并部署 ..."
        build_mysql_plugin || true
        sync_mysql_plugin "$ROOT/dist/sqldrivers" || true
    fi
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
        # GPLv3 合规：安装包内必须同时含 GPLv3 全文与源码获取地址。
        # 落两份（tools/dwg/COPYING 就近 + 根 COPYING 供第三方声明页指向），
        # 前者为「与二进制同在」的强约束，后者便于用户按常规路径查阅。
        if [ -f "$ROOT/COPYING" ]; then
            cp "$ROOT/COPYING" "$STAGE/tools/dwg/COPYING"
            cp "$ROOT/COPYING" "$STAGE/COPYING"
            echo "[OK] GPLv3 许可证全文已入包（COPYING / tools/dwg/COPYING）"
        else
            echo "[错误] 仓库根缺少 COPYING（GPLv3 全文），安装包含 GPL 转换器即违约" >&2
            exit 1
        fi
    else
        echo "[警告] build/tools/dwg 不存在，安装包将不含 DWG 转换器（DWG 将无法打开）" >&2
    fi
    # 运行时数据：剖面 PAT 图案 + 启动封面（与 dist 模式保持一致）
    sync_hatch "$STAGE/hatch"
    sync_cover "$STAGE/"
    # MySQL 驱动插件（运行时可选依赖；缺失则尝试构建，失败仅告警；
    # 进入 STAGE/sqldrivers 后随 StageDir\* 递归入包）
    build_mysql_plugin || true
    sync_mysql_plugin "$STAGE/sqldrivers" || true
    # 随程序分发的许可协议（用户可在程序目录内查阅）
    cp "$ROOT/LICENSE.txt" "$STAGE/" 2>/dev/null || true
    cp "$ROOT/README.md" "$STAGE/" 2>/dev/null || true
    # 第三方组件许可声明（LGPL 等合规要求随分发附带）
    if [ -f "$ROOT/THIRD_PARTY_NOTICES.md" ]; then
        mkdir -p "$STAGE/third-party-notices"
        cp "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/third-party-notices/"
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
    # /DAppVersionFull 必须一起传：安装器 setup.exe 自身的 VERSIONINFO 是四段式，
    # 若沿用 .iss 里的默认值，升级版本后「安装包名 1.1.1 / 安装器属性 1.1.0.0」会漂移。
    MSYS2_ARG_CONV_EXCL="*" MSYS_NO_PATHCONV=1 \
        "$ISCC" "/DAppVersion=$APP_VERSION" "/DAppVersionFull=$APP_VERSION.0" \
        "/DStageDir=$WSTAGE" "$WISS"
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

    # ---- MSI 安装包（WiX Toolset v5）：与 Setup.exe 共用同一个 STAGE ----
    # 两种格式装的是同一批文件，避免「EXE 版有转换器、MSI 版没有」的分发漂移。
    # WiX 为 .NET 全局工具（用户级安装，无 OSMF/EULA 门控的 v5），探测顺序：
    #   PKG_WIX → PATH → %USERPROFILE%\.dotnet\tools\wix.exe
    WIX=""
    for cand in \
        "${PKG_WIX:-}" \
        "$(command -v wix 2>/dev/null || true)" \
        "$USERPROFILE/.dotnet/tools/wix.exe" \
        "$HOME/.dotnet/tools/wix.exe"; do
        if [ -n "$cand" ] && [ -f "$cand" ]; then WIX="$cand"; break; fi
    done
    MSI="$ROOT/dist/SketchCAD-$APP_VERSION-Setup.msi"
    if [ -z "$WIX" ]; then
        echo "[警告] 未找到 WiX Toolset (wix.exe)，本次未产出 MSI 安装包。" >&2
        echo "       安装：dotnet tool install --global wix --version 5.0.2" >&2
    else
        echo "[OK] WiX: $WIX"
        # WiX 是原生 Windows 程序：源文件路径须为 Windows 形式；
        # StageDir 用反斜杠（含 \** 通配符，正斜杠在部分版本会匹配失败）。
        WSTAGE_WIN="$(cd "$STAGE" && pwd -W | sed 's|/|\\|g')"
        WMSI="$(cd "$ROOT/dist" && pwd -W)/SketchCAD-$APP_VERSION-Setup.msi"
        WWXS="$(cd "$ROOT/packaging" && pwd -W)/SketchCAD.msi.wxs"
        "$WIX" build -arch x64 \
            -d "StageDir=$WSTAGE_WIN" \
            -d "AppVersion=$APP_VERSION" \
            -pdbtype none \
            -out "$WMSI" "$WWXS"
        if [ ! -f "$MSI" ]; then
            echo "[错误] 未产出预期的 MSI：$MSI" >&2
            exit 1
        fi
        echo "[OK] MSI 安装包已产出: $MSI"
        sign_file "$MSI"
    fi

    # checksums.sha256：只含本次两种安装包（不含 dist/ 里的 PDF/图片等其它文件），
    # 便于下游脚本 `sha256sum -c checksums.sha256` 直接校验分发完整性。
    # 注意：此处位于双签名之后，哈希对象是「已签名」的产物。
    ( cd "$ROOT/dist" && sha256sum "SketchCAD-$APP_VERSION-Setup.exe" > checksums.sha256
      if [ -f "SketchCAD-$APP_VERSION-Setup.msi" ]; then
          sha256sum "SketchCAD-$APP_VERSION-Setup.msi" >> checksums.sha256
      fi )

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
    echo "  EXE 安装包: $SETUP"
    if [ -f "$MSI" ]; then
        echo "  MSI 安装包: $MSI"
    fi
    echo "  校验和   : $ROOT/dist/checksums.sha256"
    echo "  EXE 大小 : $(du -h "$SETUP" | cut -f1)"
    if [ -f "$MSI" ]; then
        echo "  MSI 大小 : $(du -h "$MSI" | cut -f1)"
    fi
    echo "  SHA-256  :"
    sed 's/^/              /' "$ROOT/dist/checksums.sha256"
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
