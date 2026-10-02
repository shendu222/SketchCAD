/****************************************************************************
** Meta object code from reading C++ file 'GraphicsView.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/GraphicsView.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'GraphicsView.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t {};
} // unnamed namespace

template <> constexpr inline auto SketchCAD::GraphicsView::qt_create_metaobjectdata<qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SketchCAD::GraphicsView",
        "cursorMoved",
        "",
        "x",
        "y",
        "z",
        "viewTransformChanged",
        "canvasPointPicked",
        "pointPickCancelled",
        "regionPickRequested",
        "regionHoverMoved",
        "regionPickCancelled",
        "regionPickParamsRequested",
        "editTextRequested",
        "id",
        "requestShowProperties",
        "onModelChanged",
        "onSelectionChanged",
        "onViewChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'cursorMoved'
        QtMocHelpers::SignalData<void(double, double, double)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 3 }, { QMetaType::Double, 4 }, { QMetaType::Double, 5 },
        }}),
        // Signal 'viewTransformChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'canvasPointPicked'
        QtMocHelpers::SignalData<void(double, double)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 3 }, { QMetaType::Double, 4 },
        }}),
        // Signal 'pointPickCancelled'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'regionPickRequested'
        QtMocHelpers::SignalData<void(double, double)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 3 }, { QMetaType::Double, 4 },
        }}),
        // Signal 'regionHoverMoved'
        QtMocHelpers::SignalData<void(double, double)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 3 }, { QMetaType::Double, 4 },
        }}),
        // Signal 'regionPickCancelled'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'regionPickParamsRequested'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'editTextRequested'
        QtMocHelpers::SignalData<void(int)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 14 },
        }}),
        // Signal 'requestShowProperties'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onModelChanged'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onSelectionChanged'
        QtMocHelpers::SlotData<void(int)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 14 },
        }}),
        // Slot 'onViewChanged'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<GraphicsView, qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SketchCAD::GraphicsView::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsView::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t>.metaTypes,
    nullptr
} };

void SketchCAD::GraphicsView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GraphicsView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->cursorMoved((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 1: _t->viewTransformChanged(); break;
        case 2: _t->canvasPointPicked((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 3: _t->pointPickCancelled(); break;
        case 4: _t->regionPickRequested((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 5: _t->regionHoverMoved((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 6: _t->regionPickCancelled(); break;
        case 7: _t->regionPickParamsRequested(); break;
        case 8: _t->editTextRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->requestShowProperties(); break;
        case 10: _t->onModelChanged(); break;
        case 11: _t->onSelectionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->onViewChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)(double , double , double )>(_a, &GraphicsView::cursorMoved, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)()>(_a, &GraphicsView::viewTransformChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)(double , double )>(_a, &GraphicsView::canvasPointPicked, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)()>(_a, &GraphicsView::pointPickCancelled, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)(double , double )>(_a, &GraphicsView::regionPickRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)(double , double )>(_a, &GraphicsView::regionHoverMoved, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)()>(_a, &GraphicsView::regionPickCancelled, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)()>(_a, &GraphicsView::regionPickParamsRequested, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)(int )>(_a, &GraphicsView::editTextRequested, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (GraphicsView::*)()>(_a, &GraphicsView::requestShowProperties, 9))
            return;
    }
}

const QMetaObject *SketchCAD::GraphicsView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SketchCAD::GraphicsView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD12GraphicsViewE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsView::qt_metacast(_clname);
}

int SketchCAD::GraphicsView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsView::qt_metacall(_c, _id, _a);
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
void SketchCAD::GraphicsView::cursorMoved(double _t1, double _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void SketchCAD::GraphicsView::viewTransformChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SketchCAD::GraphicsView::canvasPointPicked(double _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void SketchCAD::GraphicsView::pointPickCancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void SketchCAD::GraphicsView::regionPickRequested(double _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void SketchCAD::GraphicsView::regionHoverMoved(double _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void SketchCAD::GraphicsView::regionPickCancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void SketchCAD::GraphicsView::regionPickParamsRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void SketchCAD::GraphicsView::editTextRequested(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void SketchCAD::GraphicsView::requestShowProperties()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}
QT_WARNING_POP
