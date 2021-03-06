/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of PySide2.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <sbkpython.h>
#include "pysideproperty.h"
#include "pysideproperty_p.h"
#include "dynamicqmetaobject_p.h"
#include "pysidesignal.h"
#include "pysidesignal_p.h"

#include <shiboken.h>
#include <QDebug>


#define QPROPERTY_CLASS_NAME "Property"

extern "C"
{

static PyObject* qpropertyTpNew(PyTypeObject* subtype, PyObject* args, PyObject* kwds);
static int qpropertyTpInit(PyObject*, PyObject*, PyObject*);
static void qpropertyDeAlloc(PyObject* self);

//methods
static PyObject* qPropertyCall(PyObject*, PyObject*, PyObject*);
static PyObject* qPropertySetter(PyObject*, PyObject*);
static PyObject* qPropertyGetter(PyObject*, PyObject*);
static int qpropertyTraverse(PyObject* self, visitproc visit, void* arg);
static int qpropertyClear(PyObject* self);

static PyMethodDef PySidePropertyMethods[] = {
    {"setter", (PyCFunction)qPropertySetter, METH_O, 0},
    {"write", (PyCFunction)qPropertySetter, METH_O, 0},
    {"getter", (PyCFunction)qPropertyGetter, METH_O, 0},
    {"read", (PyCFunction)qPropertyGetter, METH_O, 0},
    {0, 0, 0, 0}
};

PyTypeObject PySidePropertyType = {
    PyVarObject_HEAD_INIT(0, 0)
    QPROPERTY_CLASS_NAME,      /*tp_name*/
    sizeof(PySideProperty),   /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    qpropertyDeAlloc,          /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    qPropertyCall,             /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,        /*tp_flags*/
    0,                         /*tp_doc */
    qpropertyTraverse,         /*tp_traverse */
    qpropertyClear,            /*tp_clear */
    0,                         /*tp_richcompare */
    0,                         /*tp_weaklistoffset */
    0,                         /*tp_iter */
    0,                         /*tp_iternext */
    PySidePropertyMethods,     /*tp_methods */
    0,                         /*tp_members */
    0,                         /*tp_getset */
    0,                         /*tp_base */
    0,                         /*tp_dict */
    0,                         /*tp_descr_get */
    0,                         /*tp_descr_set */
    0,                         /*tp_dictoffset */
    qpropertyTpInit,           /*tp_init */
    0,                         /*tp_alloc */
    qpropertyTpNew,            /*tp_new */
    0,                         /*tp_free */
    0,                         /*tp_is_gc */
    0,                         /*tp_bases */
    0,                         /*tp_mro */
    0,                         /*tp_cache */
    0,                         /*tp_subclasses */
    0,                         /*tp_weaklist */
    0,                         /*tp_del */
    0                          /*tp_version_tag */
};

static void qpropertyMetaCall(PySideProperty* pp, PyObject* self, QMetaObject::Call call, void** args)
{
    Shiboken::Conversions::SpecificConverter converter(pp->d->typeName);
    Q_ASSERT(converter);

    QByteArray type(pp->d->typeName);

    switch(call) {
        case QMetaObject::ReadProperty:
        {
            Shiboken::GilState gil;
            PyObject* value = PySide::Property::getValue(pp, self);
            if (value) {
                converter.toCpp(value, args[0]);
                Py_DECREF(value);
            }
            break;
        }

        case QMetaObject::WriteProperty:
        {
            Shiboken::GilState gil;
            Shiboken::AutoDecRef value(converter.toPython(args[0]));
            PySide::Property::setValue(pp, self, value);
            break;
        }

        case QMetaObject::ResetProperty:
        {
            Shiboken::GilState gil;
            PySide::Property::reset(pp, self);
            break;
        }

        case QMetaObject::QueryPropertyDesignable:
        case QMetaObject::QueryPropertyScriptable:
        case QMetaObject::QueryPropertyStored:
        case QMetaObject::QueryPropertyEditable:
        case QMetaObject::QueryPropertyUser:
        // just to avoid gcc warnings
        case QMetaObject::InvokeMetaMethod:
        case QMetaObject::CreateInstance:
        case QMetaObject::IndexOfMethod:
        case QMetaObject::RegisterPropertyMetaType:
        case QMetaObject::RegisterMethodArgumentMetaType:
            break;
    }
}


static PyObject *qpropertyTpNew(PyTypeObject *subtype, PyObject * /* args */, PyObject * /* kwds */)
{
    PySideProperty* me = reinterpret_cast<PySideProperty*>(subtype->tp_alloc(subtype, 0));
    me->d = new PySidePropertyPrivate;
    memset(me->d, 0, sizeof(PySidePropertyPrivate));
    PySidePropertyPrivate* pData = me->d;
    pData->designable = true;
    pData->scriptable = true;
    pData->stored = true;
    return reinterpret_cast<PyObject *>(me);
}

int qpropertyTpInit(PyObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* type = 0;
    PySideProperty* data = reinterpret_cast<PySideProperty*>(self);
    PySidePropertyPrivate* pData = data->d;
    pData->metaCallHandler = &qpropertyMetaCall;

    static const char *kwlist[] = {"type", "fget", "fset", "freset", "fdel", "doc", "notify",
                                   "designable", "scriptable", "stored", "user",
                                   "constant", "final", 0};
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O|OOOOsObbbbbb:QtCore.QProperty", (char**) kwlist,
                                     /*OO*/     &type, &(pData->fget),
                                     /*OOO*/    &(pData->fset), &(pData->freset), &(pData->fdel),
                                     /*s*/      &(pData->doc),
                                     /*O*/      &(pData->notify),
                                     /*bbbbbb*/ &(pData->designable), &(pData->scriptable), &(pData->stored), &(pData->user), &(pData->constant), &(pData->final))) {
        return 0;
    }


    pData->typeName = PySide::Signal::getTypeName(type);

    if (!pData->typeName)
        PyErr_SetString(PyExc_TypeError, "Invalid property type or type name.");
    else if (pData->constant && (pData->fset || pData->notify))
        PyErr_SetString(PyExc_TypeError, "A constant property cannot have a WRITE method or a NOTIFY signal.");

    if (!PyErr_Occurred()) {
        Py_XINCREF(pData->fget);
        Py_XINCREF(pData->fset);
        Py_XINCREF(pData->freset);
        Py_XINCREF(pData->fdel);
        Py_XINCREF(pData->notify);
        return 1;
    } else {
        pData->fget = 0;
        pData->fset = 0;
        pData->freset = 0;
        pData->fdel = 0;
        pData->notify = 0;
        return -1;
    }
}

void qpropertyDeAlloc(PyObject* self)
{
    qpropertyClear(self);
    Py_TYPE(self)->tp_free(self);
}

PyObject *qPropertyCall(PyObject *self, PyObject *args, PyObject * /* kw */)
{
    PyObject *callback = PyTuple_GetItem(args, 0);
    if (PyFunction_Check(callback)) {
        PySideProperty *prop = reinterpret_cast<PySideProperty*>(self);
        PySidePropertyPrivate* pData = prop->d;

        Py_INCREF(callback);
        pData->fget = callback;

        Py_INCREF(self);
        return self;
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid property usage.");
        return 0;
    }
}

PyObject* qPropertySetter(PyObject* self, PyObject* callback)
{
    if (PyFunction_Check(callback)) {
        PySideProperty *prop = reinterpret_cast<PySideProperty*>(self);
        PySidePropertyPrivate* pData = prop->d;

        Py_INCREF(callback);
        pData->fset = callback;

        Py_INCREF(callback);
        return callback;
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid property setter agument.");
        return 0;
    }
}

PyObject* qPropertyGetter(PyObject* self, PyObject* callback)
{
    if (PyFunction_Check(callback)) {
        PySideProperty *prop = reinterpret_cast<PySideProperty*>(self);
        PySidePropertyPrivate* pData = prop->d;

        Py_INCREF(callback);
        pData->fget = callback;

        Py_INCREF(callback);
        return callback;
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid property getter agument.");
        return 0;
    }
}

static int qpropertyTraverse(PyObject* self, visitproc visit, void* arg)
{
    PySidePropertyPrivate* data = reinterpret_cast<PySideProperty*>(self)->d;
    if (!data)
        return 0;

    Py_VISIT(data->fget);
    Py_VISIT(data->fset);
    Py_VISIT(data->freset);
    Py_VISIT(data->fdel);
    Py_VISIT(data->notify);
    return 0;
}

static int qpropertyClear(PyObject* self)
{
    PySidePropertyPrivate* data = reinterpret_cast<PySideProperty*>(self)->d;
    if (!data)
        return 0;

    Py_CLEAR(data->fget);
    Py_CLEAR(data->fset);
    Py_CLEAR(data->freset);
    Py_CLEAR(data->fdel);
    Py_CLEAR(data->notify);


    free(data->typeName);
    free(data->doc);
    free(data->notifySignature);
    delete data;
    reinterpret_cast<PySideProperty*>(self)->d = 0;
    return 0;
}

} // extern "C"

namespace {

static PyObject* getFromType(PyTypeObject* type, PyObject* name)
{
    PyObject* attr = 0;
    attr = PyDict_GetItem(type->tp_dict, name);
    if (!attr) {
        PyObject* bases = type->tp_bases;
        int size = PyTuple_GET_SIZE(bases);
        for(int i=0; i < size; i++) {
            PyObject* base = PyTuple_GET_ITEM(bases, i);
            attr = getFromType(reinterpret_cast<PyTypeObject*>(base), name);
            if (attr)
                return attr;
        }
    }
    return attr;
}

} //namespace


namespace PySide { namespace Property {

void init(PyObject* module)
{
    if (PyType_Ready(&PySidePropertyType) < 0)
        return;

    Py_INCREF(&PySidePropertyType);
    PyModule_AddObject(module, QPROPERTY_CLASS_NAME, reinterpret_cast<PyObject *>(&PySidePropertyType));
}

bool checkType(PyObject* pyObj)
{
    if (pyObj) {
        return PyType_IsSubtype(pyObj->ob_type, &PySidePropertyType);
    }
    return false;
}

bool isPropertyType(PyObject* pyObj)
{
    return checkType(pyObj);
}

int setValue(PySideProperty* self, PyObject* source, PyObject* value)
{
    PyObject* fset = self->d->fset;
    if (fset) {
        Shiboken::AutoDecRef args(PyTuple_New(2));
        PyTuple_SET_ITEM(args, 0, source);
        PyTuple_SET_ITEM(args, 1, value);
        Py_INCREF(source);
        Py_INCREF(value);
        Shiboken::AutoDecRef result(PyObject_CallObject(fset, args));
        return (result.isNull() ? -1 : 0);
    } else {
        PyErr_SetString(PyExc_AttributeError, "Attibute read only");
    }
    return -1;
}

PyObject* getValue(PySideProperty* self, PyObject* source)
{
    PyObject* fget = self->d->fget;
    if (fget) {
        Shiboken::AutoDecRef args(PyTuple_New(1));
        Py_INCREF(source);
        PyTuple_SET_ITEM(args, 0, source);
        return  PyObject_CallObject(fget, args);
    }
    return 0;
}

int reset(PySideProperty* self, PyObject* source)
{
    PyObject* freset = self->d->freset;
    if (freset) {
        Shiboken::AutoDecRef args(PyTuple_New(1));
        Py_INCREF(source);
        PyTuple_SET_ITEM(args, 0, source);
        Shiboken::AutoDecRef result(PyObject_CallObject(freset, args));
        return (result.isNull() ? -1 : 0);
    }
    return -1;
}

const char* getTypeName(const PySideProperty* self)
{
    return self->d->typeName;
}

PySideProperty* getObject(PyObject* source, PyObject* name)
{
    PyObject* attr = 0;

    if (Shiboken::Object::isUserType(source)) {
        PyObject* dict = reinterpret_cast<SbkObject*>(source)->ob_dict;
        if (dict)
            attr = PyDict_GetItem(dict, name);
    }

    attr = getFromType(source->ob_type, name);
    if (attr && checkType(attr)) {
        Py_INCREF(attr);
        return reinterpret_cast<PySideProperty*>(attr);
    }

    if (!attr)
        PyErr_Clear(); //Clear possible error caused by PyObject_GenericGetAttr

    return 0;
}

bool isReadable(const PySideProperty * /* self */)
{
    return true;
}

bool isWritable(const PySideProperty* self)
{
    return (self->d->fset != 0);
}

bool hasReset(const PySideProperty* self)
{
    return (self->d->freset != 0);
}

bool isDesignable(const PySideProperty* self)
{
    return self->d->designable;
}

bool isScriptable(const PySideProperty* self)
{
    return self->d->scriptable;
}

bool isStored(const PySideProperty* self)
{
    return self->d->stored;
}

bool isUser(const PySideProperty* self)
{
    return self->d->user;
}

bool isConstant(const PySideProperty* self)
{
    return self->d->constant;
}

bool isFinal(const PySideProperty* self)
{
    return self->d->final;
}

const char* getNotifyName(PySideProperty* self)
{
    if (!self->d->notifySignature) {
        PyObject* str = PyObject_Str(self->d->notify);
        self->d->notifySignature = strdup(Shiboken::String::toCString(str));
        Py_DECREF(str);
    }

    return self->d->notifySignature;
}

void setMetaCallHandler(PySideProperty* self, MetaCallHandler handler)
{
    self->d->metaCallHandler = handler;
}

void setTypeName(PySideProperty* self, const char* typeName)
{
    self->d->typeName = strdup(typeName);
}

void setUserData(PySideProperty* self, void* data)
{
    self->d->userData = data;
}

void* userData(PySideProperty* self)
{
    return self->d->userData;
}

} //namespace Property
} //namespace PySide
