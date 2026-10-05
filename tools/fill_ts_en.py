# -*- coding: utf-8 -*-
"""一次性工具：为 sketchcad_en.ts 填入英文译文（按 source 精确匹配）。"""
import xml.etree.ElementTree as ET

TS = r"D:\01-CAD\02-avticationCAD\sketch-cad-cpp\resources\translations\sketchcad_en.ts"

TR = {
    "SketchCAD::MainWindow": {
        "（空）": "(Empty)",
        "清除最近文件历史": "Clear Recent File History",
        "文件(&F)": "&File",
        "新建": "&New",
        "填写标题栏...": "Fill in Title Block...",
        "插入标准标题栏...": "Insert Standard Title Block...",
        "按幅面生成图框与标题栏...": "Generate Frame and Title Block by Sheet Size...",
        "绘图比例...": "Drawing Scale...",
        "单位与图纸界限...": "Units and Drawing Limits...",
        "捕捉设置...": "Object Snap Settings...",
        "自动保存设置...": "Autosave Settings...",
        "打开...": "&Open...",
        "保存...": "&Save...",
        "最近打开(&R)": "Open &Recent",
        "导出 DXF...": "Export &DXF...",
        "退出": "E&xit",
        "打印(&P)": "&Print",
        "打印设置...": "Plot Settings...",
        "打印预览...": "Plot Preview...",
        "导出 PDF...": "Export PDF...",
        "导出 PDF（全部布局）...": "Export PDF (All Layouts)...",
        "发布... (PUBLISH)": "Publish... (PUBLISH)",
        "追踪(&K)": "&Tracking",
        "编辑(&E)": "&Edit",
        "剪贴板(&L)": "C&lipboard",
        "选择工具(&S)": "&Selection Tools",
        "修改(&M)": "&Modify",
        "阵列(&R)": "A&rray",
        "对齐(&A)": "&Align",
        "块(&B)": "&Block",
        "选择(&N)": "&Selection",
        "绘图(&D)": "&Draw",
        "测量查询(&I)": "Measure && Inquiry",
        "视图(&V)": "&View",
        "视图方向": "View Direction",
        "面板": "Panels",
        "脚本(&S)": "Scri&pt",
        "界面(&U)": "&Interface",
        "工作空间": "Workspaces",
        "插件(&X)": "Pl&ugins",
        "已加载插件": "Loaded Plugins",
        "工具(&T)": "&Tools",
        "帮助(&H)": "&Help",
        "尺寸标注": "Dimensions",
        "线宽": "Lineweight",
    },
    "SketchCAD::OptionsDialog": {
        "选项": "Options",
        "分类": "Category",
        "重置为默认值": "Reset to Defaults",
        "确定": "OK",
        "取消": "Cancel",
        "应用": "Apply",
        "常规": "General",
        "显示": "Display",
        "文件位置": "File Locations",
        "自动保存": "Autosave",
        "打印": "Plot",
        "用户": "User",
        "跟随系统": "Follow System",
        "界面语言：": "UI Language:",
    },
}

tree = ET.parse(TS)
root = tree.getroot()
filled = missing = 0
for ctx in root.findall("context"):
    cname = ctx.findtext("name")
    table = TR.get(cname, {})
    for msg in ctx.findall("message"):
        src = msg.findtext("source")
        tr = msg.find("translation")
        if src in table:
            tr.text = table[src]
            tr.attrib.pop("type", None)
            filled += 1
        else:
            missing += 1
            print("no translation for [%s] %r" % (cname, src))

tree.write(TS, encoding="utf-8", xml_declaration=True)
print("filled=%d missing=%d" % (filled, missing))
