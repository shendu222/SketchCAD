#pragma once
// ============================================================================
// 由 CMake configure_file 自动生成 —— 请勿手工编辑。
// 源模板：resources/version.h.in
// 唯一来源：CMakeLists.txt 的 project(SketchCAD VERSION 1.1.1)
// ============================================================================

#define SKETCHCAD_VERSION       "1.1.1"
#define SKETCHCAD_VERSION_MAJOR 1
#define SKETCHCAD_VERSION_MINOR 1
#define SKETCHCAD_VERSION_PATCH 1

// Windows 文件属性（任务管理器 → 详细信息）显示的四段式版本号。
// 供 C++ 侧（关于对话框）使用时须是**字符串字面量**，故带引号；
// app.rc 里同样的值由 CMake 变量直接注入为裸数字，两处共用同一个 CMake 变量，
// 不会漂移。若 C++ 侧需要数值比较，请用上面三段式的 MAJOR/MINOR/PATCH。
#define SKETCHPAD_VERSION       "1.1.1.0"
#define SKETCHPAD_PRODUCTVER    "1.1.1.0"

#define SKETCHCAD_COMPANY_NAME  "我是慎独"
#define SKETCHCAD_PRODUCT_NAME  "SketchCAD 草图设计系统"
#define SKETCHCAD_COPYRIGHT     "Copyright (C) 2026 我是慎独. All rights reserved."
