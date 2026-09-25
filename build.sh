#!/usr/bin/env bash
# 航空CAD 草图绘制模块 — 一键构建脚本（Windows / Git Bash + Qt6 MinGW）
#
# 用法：
#   ./build.sh          增量构建
#   ./build.sh clean    清理后完整重建
#   ./build.sh test     构建并运行核心层自测
#   ./build.sh dist     构建并打包可独立运行的 dist/ 目录
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

if [ "$MODE" = "test" ]; then
    echo "---- 核心层自测 ----"
    # Qt 在 MSYS 伪终端下默认不写 stderr，需显式声明有控制台
    QT_ASSUME_STDERR_HAS_CONSOLE=1 "$ROOT/build/SketchSelfTest.exe"
fi

if [ "$MODE" = "dist" ]; then
    mkdir -p "$ROOT/dist"
    cp "$ROOT/build/SketchCAD.exe" "$ROOT/dist/"
    "$QT_ROOT/bin/windeployqt.exe" --release --no-translations \
        --no-system-d3d-compiler --no-opengl-sw "$WROOT/dist/SketchCAD.exe" >/dev/null
    sync_hatch "$ROOT/dist/hatch"
    echo "[OK] 已打包: $ROOT/dist/SketchCAD.exe（含 Qt 运行时，可拷贝到无 Qt 环境运行）"
    echo "[OK] 剖面 PAT 图案已同步到 dist/hatch/"
fi
