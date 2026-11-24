/****************************************************************************
** Meta object code from reading C++ file 'CrashRecoveryDialog.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/CrashRecoveryDialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CrashRecoveryDialog.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto SketchCAD::CrashRecoveryDialog::qt_create_metaobjectdata<qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SketchCAD::CrashRecoveryDialog",
        "recoverRequested",
        "",
        "SketchCAD::AutoSave::Snapshot",
        "s",
        "discardRequested",
        "discardAllRequested",
        "onRecover",
        "onDiscard",
        "onDiscardAll"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'recoverRequested'
        QtMocHelpers::SignalData<void(const SketchCAD::AutoSave::Snapshot &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'discardRequested'
        QtMocHelpers::SignalData<void(const SketchCAD::AutoSave::Snapshot &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'discardAllRequested'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onRecover'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDiscard'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDiscardAll'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<CrashRecoveryDialog, qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SketchCAD::CrashRecoveryDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t>.metaTypes,
    nullptr
} };

void SketchCAD::CrashRecoveryDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CrashRecoveryDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->recoverRequested((*reinterpret_cast<std::add_pointer_t<SketchCAD::AutoSave::Snapshot>>(_a[1]))); break;
        case 1: _t->discardRequested((*reinterpret_cast<std::add_pointer_t<SketchCAD::AutoSave::Snapshot>>(_a[1]))); break;
        case 2: _t->discardAllRequested(); break;
        case 3: _t->onRecover(); break;
        case 4: _t->onDiscard(); break;
        case 5: _t->onDiscardAll(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CrashRecoveryDialog::*)(const SketchCAD::AutoSave::Snapshot & )>(_a, &CrashRecoveryDialog::recoverRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (CrashRecoveryDialog::*)(const SketchCAD::AutoSave::Snapshot & )>(_a, &CrashRecoveryDialog::discardRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (CrashRecoveryDialog::*)()>(_a, &CrashRecoveryDialog::discardAllRequested, 2))
            return;
    }
}

const QMetaObject *SketchCAD::CrashRecoveryDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SketchCAD::CrashRecoveryDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SketchCAD19CrashRecoveryDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int SketchCAD::CrashRecoveryDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void SketchCAD::CrashRecoveryDialog::recoverRequested(const SketchCAD::AutoSave::Snapshot & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SketchCAD::CrashRecoveryDialog::discardRequested(const SketchCAD::AutoSave::Snapshot & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SketchCAD::CrashRecoveryDialog::discardAllRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
