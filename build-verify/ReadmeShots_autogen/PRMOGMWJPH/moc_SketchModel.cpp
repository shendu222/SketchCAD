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
        "ucsChanged",
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
        "dimensionDrivenChanged",
        "commandPromptChanged",
        "prompt",
        "commandOptionsChanged",
        "opts",
        "showLineWeightChanged",
        "dynModeChanged",
        "scaleChanged",
        "ratio",
        "ltscaleChanged",
        "layersChanged",
        "currentLayerChanged",
        "name",
        "blocksChanged",
        "textStylesChanged",
        "dimStylesChanged",
        "layoutsChanged",
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
        // Signal 'ucsChanged'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gridToggled'
        QtMocHelpers::SignalData<void(bool)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 19 },
        }}),
        // Signal 'orthoModeChanged'
        QtMocHelpers::SignalData<void(bool)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Signal 'gridSnapChanged'
        QtMocHelpers::SignalData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cursorMoved'
        QtMocHelpers::SignalData<void(double, double, double)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 24 }, { QMetaType::Double, 25 }, { QMetaType::Double, 26 },
        }}),
        // Signal 'defaultStyleChanged'
        QtMocHelpers::SignalData<void(const QColor &, double)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 28, 29 }, { QMetaType::Double, 30 },
        }}),
        // Signal 'defaultLineStyleChanged'
        QtMocHelpers::SignalData<void(int)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 32 },
        }}),
        // Signal 'chainLineToggled'
        QtMocHelpers::SignalData<void(bool)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Signal 'lineParamsChanged'
        QtMocHelpers::SignalData<void()>(34, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dimensionStandardChanged'
        QtMocHelpers::SignalData<void(DimensionStandard)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 36, 37 },
        }}),
        // Signal 'dimensionDrivenChanged'
        QtMocHelpers::SignalData<void(bool)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Signal 'commandPromptChanged'
        QtMocHelpers::SignalData<void(const QString &)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 40 },
        }}),
        // Signal 'commandOptionsChanged'
        QtMocHelpers::SignalData<void(const QStringList &)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 42 },
        }}),
        // Signal 'showLineWeightChanged'
        QtMocHelpers::SignalData<void(bool)>(43, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Signal 'dynModeChanged'
        QtMocHelpers::SignalData<void(bool)>(44, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Signal 'scaleChanged'
        QtMocHelpers::SignalData<void(double)>(45, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 46 },
        }}),
        // Signal 'ltscaleChanged'
        QtMocHelpers::SignalData<void(double)>(47, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 46 },
        }}),
        // Signal 'layersChanged'
        QtMocHelpers::SignalData<void()>(48, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentLayerChanged'
        QtMocHelpers::SignalData<void(const QString &)>(49, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 50 },
        }}),
        // Signal 'blocksChanged'
        QtMocHelpers::SignalData<void()>(51, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'textStylesChanged'
        QtMocHelpers::SignalData<void()>(52, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dimStylesChanged'
        QtMocHelpers::SignalData<void()>(53, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'layoutsChanged'
        QtMocHelpers::SignalData<void()>(54, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'unitsChanged'
        QtMocHelpers::SignalData<void()>(55, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'limitsChanged'
        QtMocHelpers::SignalData<void()>(56, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'snapConfigChanged'
        QtMocHelpers::SignalData<void()>(57, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trackConfigChanged'
        QtMocHelpers::SignalData<void()>(58, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trackPointsChanged'
        QtMocHelpers::SignalData<void()>(59, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'constraintsChanged'
        QtMocHelpers::SignalData<void()>(60, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'paramsChanged'
        QtMocHelpers::SignalData<void()>(61, 2, QMC::AccessPublic, QMetaType::Void),
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
        case 8: _t->ucsChanged(); break;
        case 9: _t->gridToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->orthoModeChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->gridSnapChanged(); break;
        case 12: _t->cursorMoved((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 13: _t->defaultStyleChanged((*reinterpret_cast<std::add_pointer_t<QColor>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 14: _t->defaultLineStyleChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->chainLineToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->lineParamsChanged(); break;
        case 17: _t->dimensionStandardChanged((*reinterpret_cast<std::add_pointer_t<DimensionStandard>>(_a[1]))); break;
        case 18: _t->dimensionDrivenChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 19: _t->commandPromptChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->commandOptionsChanged((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 21: _t->showLineWeightChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 22: _t->dynModeChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 23: _t->scaleChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 24: _t->ltscaleChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 25: _t->layersChanged(); break;
        case 26: _t->currentLayerChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->blocksChanged(); break;
        case 28: _t->textStylesChanged(); break;
        case 29: _t->dimStylesChanged(); break;
        case 30: _t->layoutsChanged(); break;
        case 31: _t->unitsChanged(); break;
        case 32: _t->limitsChanged(); break;
        case 33: _t->snapConfigChanged(); break;
        case 34: _t->trackConfigChanged(); break;
        case 35: _t->trackPointsChanged(); break;
        case 36: _t->constraintsChanged(); break;
        case 37: _t->paramsChanged(); break;
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
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::ucsChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::gridToggled, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::orthoModeChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::gridSnapChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(double , double , double )>(_a, &SketchModel::cursorMoved, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QColor & , double )>(_a, &SketchModel::defaultStyleChanged, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(int )>(_a, &SketchModel::defaultLineStyleChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::chainLineToggled, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::lineParamsChanged, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(DimensionStandard )>(_a, &SketchModel::dimensionStandardChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::dimensionDrivenChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & )>(_a, &SketchModel::commandPromptChanged, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QStringList & )>(_a, &SketchModel::commandOptionsChanged, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::showLineWeightChanged, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(bool )>(_a, &SketchModel::dynModeChanged, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(double )>(_a, &SketchModel::scaleChanged, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(double )>(_a, &SketchModel::ltscaleChanged, 24))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::layersChanged, 25))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)(const QString & )>(_a, &SketchModel::currentLayerChanged, 26))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::blocksChanged, 27))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::textStylesChanged, 28))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::dimStylesChanged, 29))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::layoutsChanged, 30))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::unitsChanged, 31))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::limitsChanged, 32))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::snapConfigChanged, 33))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::trackConfigChanged, 34))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::trackPointsChanged, 35))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::constraintsChanged, 36))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchModel::*)()>(_a, &SketchModel::paramsChanged, 37))
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
        if (_id < 38)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 38;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 38)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 38;
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
void SketchCAD::SketchModel::ucsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void SketchCAD::SketchModel::gridToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void SketchCAD::SketchModel::orthoModeChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void SketchCAD::SketchModel::gridSnapChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void SketchCAD::SketchModel::cursorMoved(double _t1, double _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1, _t2, _t3);
}

// SIGNAL 13
void SketchCAD::SketchModel::defaultStyleChanged(const QColor & _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1, _t2);
}

// SIGNAL 14
void SketchCAD::SketchModel::defaultLineStyleChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 14, nullptr, _t1);
}

// SIGNAL 15
void SketchCAD::SketchModel::chainLineToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1);
}

// SIGNAL 16
void SketchCAD::SketchModel::lineParamsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void SketchCAD::SketchModel::dimensionStandardChanged(DimensionStandard _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 17, nullptr, _t1);
}

// SIGNAL 18
void SketchCAD::SketchModel::dimensionDrivenChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 18, nullptr, _t1);
}

// SIGNAL 19
void SketchCAD::SketchModel::commandPromptChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 19, nullptr, _t1);
}

// SIGNAL 20
void SketchCAD::SketchModel::commandOptionsChanged(const QStringList & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 20, nullptr, _t1);
}

// SIGNAL 21
void SketchCAD::SketchModel::showLineWeightChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 21, nullptr, _t1);
}

// SIGNAL 22
void SketchCAD::SketchModel::dynModeChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 22, nullptr, _t1);
}

// SIGNAL 23
void SketchCAD::SketchModel::scaleChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 23, nullptr, _t1);
}

// SIGNAL 24
void SketchCAD::SketchModel::ltscaleChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 24, nullptr, _t1);
}

// SIGNAL 25
void SketchCAD::SketchModel::layersChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 25, nullptr);
}

// SIGNAL 26
void SketchCAD::SketchModel::currentLayerChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 26, nullptr, _t1);
}

// SIGNAL 27
void SketchCAD::SketchModel::blocksChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 27, nullptr);
}

// SIGNAL 28
void SketchCAD::SketchModel::textStylesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 28, nullptr);
}

// SIGNAL 29
void SketchCAD::SketchModel::dimStylesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 29, nullptr);
}

// SIGNAL 30
void SketchCAD::SketchModel::layoutsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 30, nullptr);
}

// SIGNAL 31
void SketchCAD::SketchModel::unitsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 31, nullptr);
}

// SIGNAL 32
void SketchCAD::SketchModel::limitsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 32, nullptr);
}

// SIGNAL 33
void SketchCAD::SketchModel::snapConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 33, nullptr);
}

// SIGNAL 34
void SketchCAD::SketchModel::trackConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 34, nullptr);
}

// SIGNAL 35
void SketchCAD::SketchModel::trackPointsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 35, nullptr);
}

// SIGNAL 36
void SketchCAD::SketchModel::constraintsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 36, nullptr);
}

// SIGNAL 37
void SketchCAD::SketchModel::paramsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 37, nullptr);
}
QT_WARNING_POP
