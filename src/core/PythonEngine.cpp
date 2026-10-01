#include "PythonEngine.h"
#include "ScriptApi.h"

// 仅在开启 Python 开关时编译本文件（默认 OFF，不进入 CORE_SOURCES）。
#ifdef SCRIPTHOST_ENABLE_PYTHON

#include <QString>

namespace SketchCAD {

// 当前执行上下文（单线程宿主，顺序执行，故用全局指针承载交互面与日志回调）。
static ScriptApi*                    g_pyApi  = nullptr;
static std::function<void(QString)>  g_pySink;

// ---- sketch 模块函数实现（C 层桥接到 ScriptApi）----
static PyObject* py_add_line(PyObject*, PyObject* args) {
    double x1, y1, x2, y2;
    if (!PyArg_ParseTuple(args, "dddd", &x1, &y1, &x2, &y2)) return nullptr;
    return PyLong_FromLong(g_pyApi->addLine(x1, y1, x2, y2));
}
static PyObject* py_add_circle(PyObject*, PyObject* args) {
    double cx, cy, r;
    if (!PyArg_ParseTuple(args, "ddd", &cx, &cy, &r)) return nullptr;
    return PyLong_FromLong(g_pyApi->addCircle(cx, cy, r));
}
static PyObject* py_add_arc(PyObject*, PyObject* args) {
    double cx, cy, r, a0, a1;
    if (!PyArg_ParseTuple(args, "ddddd", &cx, &cy, &r, &a0, &a1)) return nullptr;
    return PyLong_FromLong(g_pyApi->addArc(cx, cy, r, a0, a1));
}
static PyObject* py_add_rect(PyObject*, PyObject* args) {
    double x, y, w, h;
    if (!PyArg_ParseTuple(args, "dddd", &x, &y, &w, &h)) return nullptr;
    return PyLong_FromLong(g_pyApi->addRect(x, y, w, h));
}
static PyObject* py_add_point(PyObject*, PyObject* args) {
    double x, y;
    if (!PyArg_ParseTuple(args, "dd", &x, &y)) return nullptr;
    return PyLong_FromLong(g_pyApi->addPoint(x, y));
}
static PyObject* py_add_text(PyObject*, PyObject* args) {
    double x, y, h;
    const char* t = "";
    if (!PyArg_ParseTuple(args, "ddsd", &x, &y, &t, &h)) return nullptr;
    return PyLong_FromLong(g_pyApi->addText(x, y, QString::fromUtf8(t), h));
}
static PyObject* py_element_count(PyObject*) {
    return PyLong_FromLong(g_pyApi->elementCount());
}
static PyObject* py_element_info(PyObject*, PyObject* args) {
    int id;
    if (!PyArg_ParseTuple(args, "i", &id)) return nullptr;
    return PyUnicode_FromString(g_pyApi->elementInfo(id).toUtf8().constData());
}
static PyObject* py_remove_element(PyObject*, PyObject* args) {
    int id;
    if (!PyArg_ParseTuple(args, "i", &id)) return nullptr;
    return PyBool_FromLong(g_pyApi->removeElement(id) ? 1 : 0);
}
static PyObject* py_run(PyObject*, PyObject* args) {
    const char* c = "";
    if (!PyArg_ParseTuple(args, "s", &c)) return nullptr;
    return PyBool_FromLong(g_pyApi->run(QString::fromUtf8(c)) ? 1 : 0);
}
static PyObject* py_log(PyObject*, PyObject* args) {
    const char* s = "";
    if (!PyArg_ParseTuple(args, "s", &s)) return nullptr;
    QString q = QString::fromUtf8(s);
    g_pyApi->log(q);              // 经 ScriptApi::log 发射 scriptOutput，由引擎捕获
    if (g_pySink) g_pySink(q);
    Py_RETURN_NONE;
}
static PyObject* py_version(PyObject*) {
    return PyUnicode_FromString(g_pyApi->version().toUtf8().constData());
}

static PyMethodDef kSketchMethods[] = {
    {"add_line",       py_add_line,       METH_VARARGS, "add_line(x1,y1,x2,y2) -> id"},
    {"add_circle",     py_add_circle,     METH_VARARGS, "add_circle(cx,cy,r) -> id"},
    {"add_arc",        py_add_arc,        METH_VARARGS, "add_arc(cx,cy,r,a0,a1) -> id"},
    {"add_rect",       py_add_rect,       METH_VARARGS, "add_rect(x,y,w,h) -> id"},
    {"add_point",      py_add_point,      METH_VARARGS, "add_point(x,y) -> id"},
    {"add_text",       py_add_text,       METH_VARARGS, "add_text(x,y,text,h) -> id"},
    {"element_count",  py_element_count,  METH_NOARGS,  "element_count() -> int"},
    {"element_info",   py_element_info,    METH_VARARGS, "element_info(id) -> str"},
    {"remove_element", py_remove_element,  METH_VARARGS, "remove_element(id) -> bool"},
    {"run",            py_run,             METH_VARARGS, "run(command) -> bool"},
    {"log",            py_log,             METH_VARARGS, "log(msg)"},
    {"version",        py_version,         METH_NOARGS,  "version() -> str"},
    {nullptr, nullptr, 0, nullptr}
};

bool PythonEngine::available() const {
    // 编译期已链接 CPython，则默认可用；真正初始化在 execute 中按需进行。
    return true;
}

ScriptResult PythonEngine::execute(const QString& code, ScriptApi* api,
                                   std::function<void(const QString&)> logSink) {
    ScriptResult res;
    res.language = language();
    if (!api) { res.error = QStringLiteral("内部交互面未初始化"); return res; }

    if (!Py_IsInitialized()) Py_Initialize();
    g_pyApi  = api;
    g_pySink = logSink;

    // 捕获脚本 print/log 输出：ScriptApi::log 会发射 scriptOutput。此处把输出累积到 res 并转发。
    QMetaObject::Connection conn = QObject::connect(api, &ScriptApi::scriptOutput, api,
        [&](const QString& s) { res.output += s + "\n"; if (logSink) logSink(s); });

    // 创建/复用 `sketch` 模块并挂上桥接函数
    PyObject* mod = PyImport_AddModule("sketch");
    if (!mod) {
        res.error = QStringLiteral("无法创建 sketch 模块");
        PyErr_Clear();
        QObject::disconnect(conn);
        g_pyApi = nullptr; g_pySink = nullptr;
        return res;
    }
    PyModule_AddFunctions(mod, kSketchMethods);

    // 重定向 stdout/stderr 到 sketch.log（逐行转发到日志流）
    const char* boot =
        "import sys\n"
        "import sketch\n"
        "class _SketchOut:\n"
        "    def write(self, s):\n"
        "        if s:\n"
        "            sketch.log(s.rstrip('\\n'))\n"
        "    def flush(self):\n"
        "        pass\n"
        "_o = _SketchOut()\n"
        "sys.stdout = _o\n"
        "sys.stderr = _o\n";
    PyRun_SimpleString(boot);

    // 在独立局部命名空间执行用户代码（复用 __main__ 的内建），捕获异常
    PyObject* main = PyImport_AddModule("__main__");
    PyObject* gdict = PyModule_GetDict(main);            // borrowed
    PyObject* ldict = PyDict_New();
    PyDict_SetItemString(ldict, "__builtins__", PyEval_GetBuiltins());

    PyObject* result = PyRun_String(code.toUtf8().constData(), Py_file_input, gdict, ldict);
    if (!result) {
        res.ok = false;
        PyObject *ptype = nullptr, *pval = nullptr, *ptb = nullptr;
        PyErr_Fetch(&ptype, &pval, &ptb);
        PyObject* msg = pval ? PyObject_Str(pval) : PyObject_Str(ptype);
        if (msg) {
            const char* c = PyUnicode_AsUTF8(msg);
            res.error = QString::fromUtf8(c ? c : "?");
            Py_XDECREF(msg);
        } else {
            res.error = QStringLiteral("Python 执行异常（无法格式化）");
        }
        Py_XDECREF(ptype); Py_XDECREF(pval); Py_XDECREF(ptb);
    } else {
        res.ok = true;
        Py_XDECREF(result);
    }

    Py_XDECREF(ldict);
    QObject::disconnect(conn);
    g_pyApi = nullptr; g_pySink = nullptr;
    return res;
}

} // namespace SketchCAD

#endif // SCRIPTHOST_ENABLE_PYTHON
