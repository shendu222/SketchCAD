/****************************************************************************
** Meta object code from reading C++ file 'SketchModel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/SketchModel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SketchModel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9SketchCAD11SketchModelE_t {};
} // unnamed namespace

template <> constexpr inline auto SketchCAD::SketchModel::qt_create_metaobjectdata<qt_meta_tag_ZN9SketchCAD11SketchModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SketchCAD::SketchModel",
        "modelChanged",
        "",
        "selectionChanged",
        "id",
        "elementUpdated",
        "logAppended",
        "msg",
        "type",
        "statusCommandChanged",
        "cmd",
        "activeToolChanged",
        "tool",
        "viewChanged",
        "planeChanged",
        "Plane",
        "p",
        "gridToggled",
        "show",
        "orthoModeChanged",
        "on",
        "gridSnapChanged",
        "cursorMoved",
        "x",
        "y",
        "z",
        "defaultStyleChanged",
        "QColor",
        "color",
        "width",
        "defaultLineStyleChanged",
        "style",
        "chainLineToggled",
        "lineParamsChanged",
        "dimensionStandardChanged",
        "DimensionStandard",
        "s",
        "commandPromptChanged",
        "prompt",
        "commandOptionsChanged",
        "opts",
        "showLineWeightChanged",
        "dynModeChanged",
        "scaleChanged",
        "ratio",
        "layersChanged",
        "currentLayerChanged",
        "name",
        "blocksChanged",
        "textStylesChanged",
        "unitsChanged",
        "limitsChanged",
        "snapConfigChanged",
        "trackConfigChanged",
        "trackPointsChanged",
        "constraintsChanged",
        "paramsChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'modelChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectionChanged'
        QtMocHelpers::SignalData<void(int)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'elementUpdated'
        QtMocHelpers::SignalData<void(int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'logAppended'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
        // Signal 'statusCommandChanged'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'activeToolChanged'
        QtMocHelpers::SignalData<void(const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Signal 'viewChanged'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'planeChanged'
        QtMocHelpers::SignalData<void(Plane)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 15, 16 },
        }}),
        // Signal 'gridToggled'
        QtMocHelpers::SignalData<void(bool)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Signal 'orthoModeChanged'
        QtMocHelpers::SignalData<void(bool)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 },
        }}),
        // Signal 'gridSnapChanged'
        QtMocHelpers::SignalData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cursorMoved'
        QtMocHelpers::SignalData<void(double, double, double)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 23 }, { QMetaType::Double, 24 }, { QMetaType::Double, 25 },
        }}),
        // Signal 'defaultStyleChanged'
        QtMocHelpers::SignalData<void(const QColor &, double)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 27, 28 }, { QMetaType::Double, 29 },
        }}),
        // Signal 'defaultLineStyleChanged'
        QtMocHelpers::SignalData<void(int)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 31 },
        }}),
        // Signal 'chainLineToggled'
        QtMocHelpers::SignalData<void(bool)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 },
        }}),
        // Signal 'lineParamsChanged'
        QtMocHelpers::SignalData<void()>(33, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dimensionStandardChanged'
        QtMocHelpers::SignalData<void(DimensionStandard)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 35, 36 },
        }}),
        // Signal 'commandPromptChanged'
        QtMocHelpers::SignalData<void(const QString &)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 38 },
        }}),
        // Signal 'commandOptionsChanged'
        QtMocHelpers::SignalData<void(const QStringList &)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 40 },
        }}),
        // Signal 'showLineWeightChanged'
        QtMocHelpers::SignalData<void(bool)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 },
        }}),
        // Signal 'dynModeChanged'
        QtMocHelpers::SignalData<void(bool)>(42, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 },
        }}),
        // Signal 'scaleChanged'
        QtMocHelpers::SignalData<void(double)>(43, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 44 },
        }}),
        // Signal 'layersChanged'
        QtMocHelpers::SignalData<void()>(45, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentLayerChanged'
        QtMocHelpers::SignalData<void(const QString &)>(46, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 47 },
        }}),
        // Signal 'blocksChanged'
        QtMocHelpers::SignalData<void()>(48, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'textStylesChanged'
        QtMocHelpers::SignalData<void()>(49, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'unitsChanged'
        QtMocHelpers::SignalData<void()>(50, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'limitsChanged'
        QtMocHelpers::SignalData<void()>(51, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'snapConfigChanged'
        QtMocHelpers::SignalData<void()>(52, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trackConfigChanged'
        QtMocHelpers::SignalData<void()>(53, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trackPointsChanged'
        QtMocHelpers::SignalData<void()>(54, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'constraintsChanged'
        QtMocHelpers::SignalData<void()>(55, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'paramsChanged'
        QtMocHelpers::SignalData<void()>(56, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SketchModel, qt_meta_tag_ZN9SketchCAD11SketchModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SketchCAD::SketchModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD11SketchModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD11SketchModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9SketchCAD11SketchModelE_t>.metaTypes,
    nullptr
} };

void SketchCAD::SketchModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SketchModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->modelChanged(); break;
        case 1: _t->selectionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->elementUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->logAppended((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->statusCommandChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->activeToolChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->viewChanged(); break;
        case 7: _t->planeChanged((*reinterpret_cast<std::add_pointer_t<Plane>>(_a[1]))); break;
        case 8: _t->gridToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 9: _t->orthoModeChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->gridSnapChanged(); break;
        case 11: _t->cursorMoved((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 12: _t->defaultStyleChanged((*reinterpret_cast<std::add_pointer_t<QColor>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 13: _t->defaultLineStyleChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 14: _t->chainLineToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->lineParamsChanged(); break;
        case 16: _t->dimensionStandardChanged((*reinterpret_cast<std::add_pointer_t<DimensionStandard>>(_a[1]))); break;
        case 17: _t->commandPromptChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->commandOptionsChanged((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 19: _t->showLineWeightChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 20: _t->dynModeChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 21: _t->scaleChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 22: _t->layersChanged(); break;
        case 23: _t->currentLayerChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->blocksChanged(); break;
        case 25: _t->textStylesChanged(); break;
        case 26: _t->unitsChanged(); break;
        case 27: _t->limitsChanged(); break;
        case 28: _t->snapConfigChanged(); break;
        case 29: _t->trackConfigChanged(); break;
        case 30: _t->trackPointsChanged(); break;
        case 31: _t->constraintsChanged(); break;
        case 32: _t->paramsChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::modelChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(int )>(_a, &SketchModel::selectionChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(int )>(_a, &SketchModel::elementUpdated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & , const QString & )>(_a, &SketchModel::logAppended, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & )>(_a, &SketchModel::statusCommandChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & )>(_a, &SketchModel::activeToolChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::viewChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(Plane )>(_a, &SketchModel::planeChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::gridToggled, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::orthoModeChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::gridSnapChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(double , double , double )>(_a, &SketchModel::cursorMoved, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QColor & , double )>(_a, &SketchModel::defaultStyleChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(int )>(_a, &SketchModel::defaultLineStyleChanged, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::chainLineToggled, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::lineParamsChanged, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(DimensionStandard )>(_a, &SketchModel::dimensionStandardChanged, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & )>(_a, &SketchModel::commandPromptChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QStringList & )>(_a, &SketchModel::commandOptionsChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::showLineWeightChanged, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::dynModeChanged, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(double )>(_a, &SketchModel::scaleChanged, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::layersChanged, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & )>(_a, &SketchModel::currentLayerChanged, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::blocksChanged, 24))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::textStylesChanged, 25))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::unitsChanged, 26))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::limitsChanged, 27))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::snapConfigChanged, 28))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::trackConfigChanged, 29))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::trackPointsChanged, 30))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::constraintsChanged, 31))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::paramsChanged, 32))
            return;
    }
}

const QMetaObject *SketchCAD::SketchModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SketchCAD::SketchModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD11SketchModelE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SketchCAD::SketchModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 33)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 33;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 33)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 33;
    }
    return _id;
}

// SIGNAL 0
void SketchCAD::SketchModel::modelChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void SketchCAD::SketchModel::selectionChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SketchCAD::SketchModel::elementUpdated(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SketchCAD::SketchModel::logAppended(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void SketchCAD::SketchModel::statusCommandChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void SketchCAD::SketchModel::activeToolChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void SketchCAD::SketchModel::viewChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void SketchCAD::SketchModel::planeChanged(Plane _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void SketchCAD::SketchModel::gridToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void SketchCAD::SketchModel::orthoModeChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void SketchCAD::SketchModel::gridSnapChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void SketchCAD::SketchModel::cursorMoved(double _t1, double _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2, _t3);
}

// SIGNAL 12
void SketchCAD::SketchModel::defaultStyleChanged(const QColor & _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1, _t2);
}

// SIGNAL 13
void SketchCAD::SketchModel::defaultLineStyleChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void SketchCAD::SketchModel::chainLineToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 14, nullptr, _t1);
}

// SIGNAL 15
void SketchCAD::SketchModel::lineParamsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void SketchCAD::SketchModel::dimensionStandardChanged(DimensionStandard _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 16, nullptr, _t1);
}

// SIGNAL 17
void SketchCAD::SketchModel::commandPromptChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 17, nullptr, _t1);
}

// SIGNAL 18
void SketchCAD::SketchModel::commandOptionsChanged(const QStringList & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 18, nullptr, _t1);
}

// SIGNAL 19
void SketchCAD::SketchModel::showLineWeightChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 19, nullptr, _t1);
}

// SIGNAL 20
void SketchCAD::SketchModel::dynModeChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 20, nullptr, _t1);
}

// SIGNAL 21
void SketchCAD::SketchModel::scaleChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 21, nullptr, _t1);
}

// SIGNAL 22
void SketchCAD::SketchModel::layersChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 22, nullptr);
}

// SIGNAL 23
void SketchCAD::SketchModel::currentLayerChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 23, nullptr, _t1);
}

// SIGNAL 24
void SketchCAD::SketchModel::blocksChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 24, nullptr);
}

// SIGNAL 25
void SketchCAD::SketchModel::textStylesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 25, nullptr);
}

// SIGNAL 26
void SketchCAD::SketchModel::unitsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 26, nullptr);
}

// SIGNAL 27
void SketchCAD::SketchModel::limitsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 27, nullptr);
}

// SIGNAL 28
void SketchCAD::SketchModel::snapConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 28, nullptr);
}

// SIGNAL 29
void SketchCAD::SketchModel::trackConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 29, nullptr);
}

// SIGNAL 30
void SketchCAD::SketchModel::trackPointsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 30, nullptr);
}

// SIGNAL 31
void SketchCAD::SketchModel::constraintsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 31, nullptr);
}

// SIGNAL 32
void SketchCAD::SketchModel::paramsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 32, nullptr);
}
QT_WARNING_POP
