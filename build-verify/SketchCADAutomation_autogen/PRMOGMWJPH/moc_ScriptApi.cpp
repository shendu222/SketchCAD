/****************************************************************************
** Meta object code from reading C++ file 'ScriptApi.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/ScriptApi.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ScriptApi.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN9SketchCAD9ScriptApiE_t {};
} // unnamed namespace

template <> constexpr inline auto SketchCAD::ScriptApi::qt_create_metaobjectdata<qt_meta_tag_ZN9SketchCAD9ScriptApiE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SketchCAD::ScriptApi",
        "scriptOutput",
        "",
        "msg",
        "addLine",
        "x1",
        "y1",
        "x2",
        "y2",
        "addCircle",
        "cx",
        "cy",
        "r",
        "addArc",
        "startDeg",
        "endDeg",
        "addRect",
        "x",
        "y",
        "w",
        "h",
        "addPoint",
        "addText",
        "text",
        "heightMm",
        "elementCount",
        "elementInfo",
        "id",
        "removeElement",
        "run",
        "command",
        "log",
        "version"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'scriptOutput'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'addLine'
        QtMocHelpers::MethodData<int(double, double, double, double)>(4, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Double, 5 }, { QMetaType::Double, 6 }, { QMetaType::Double, 7 }, { QMetaType::Double, 8 },
        }}),
        // Method 'addCircle'
        QtMocHelpers::MethodData<int(double, double, double)>(9, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Double, 10 }, { QMetaType::Double, 11 }, { QMetaType::Double, 12 },
        }}),
        // Method 'addArc'
        QtMocHelpers::MethodData<int(double, double, double, double, double)>(13, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Double, 10 }, { QMetaType::Double, 11 }, { QMetaType::Double, 12 }, { QMetaType::Double, 14 },
            { QMetaType::Double, 15 },
        }}),
        // Method 'addRect'
        QtMocHelpers::MethodData<int(double, double, double, double)>(16, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Double, 17 }, { QMetaType::Double, 18 }, { QMetaType::Double, 19 }, { QMetaType::Double, 20 },
        }}),
        // Method 'addPoint'
        QtMocHelpers::MethodData<int(double, double)>(21, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Double, 17 }, { QMetaType::Double, 18 },
        }}),
        // Method 'addText'
        QtMocHelpers::MethodData<int(double, double, const QString &, double)>(22, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Double, 17 }, { QMetaType::Double, 18 }, { QMetaType::QString, 23 }, { QMetaType::Double, 24 },
        }}),
        // Method 'elementCount'
        QtMocHelpers::MethodData<int() const>(25, 2, QMC::AccessPublic, QMetaType::Int),
        // Method 'elementInfo'
        QtMocHelpers::MethodData<QString(int) const>(26, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Int, 27 },
        }}),
        // Method 'removeElement'
        QtMocHelpers::MethodData<bool(int)>(28, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 27 },
        }}),
        // Method 'run'
        QtMocHelpers::MethodData<bool(const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 30 },
        }}),
        // Method 'log'
        QtMocHelpers::MethodData<void(const QString &)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'version'
        QtMocHelpers::MethodData<QString() const>(32, 2, QMC::AccessPublic, QMetaType::QString),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ScriptApi, qt_meta_tag_ZN9SketchCAD9ScriptApiE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SketchCAD::ScriptApi::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD9ScriptApiE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD9ScriptApiE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9SketchCAD9ScriptApiE_t>.metaTypes,
    nullptr
} };

void SketchCAD::ScriptApi::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ScriptApi *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->scriptOutput((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: { int _r = _t->addLine((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 2: { int _r = _t->addCircle((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 3: { int _r = _t->addArc((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 4: { int _r = _t->addRect((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 5: { int _r = _t->addPoint((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 6: { int _r = _t->addText((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 7: { int _r = _t->elementCount();
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 8: { QString _r = _t->elementInfo((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->removeElement((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 10: { bool _r = _t->run((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->log((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: { QString _r = _t->version();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ScriptApi::*)(const QString & )>(_a, &ScriptApi::scriptOutput, 0))
            return;
    }
}

const QMetaObject *SketchCAD::ScriptApi::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SketchCAD::ScriptApi::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD9ScriptApiE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SketchCAD::ScriptApi::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void SketchCAD::ScriptApi::scriptOutput(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
