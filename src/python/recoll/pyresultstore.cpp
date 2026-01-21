/* Copyright (C) 2007-2020 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <Python.h>
#include <structmember.h>
#include <bytesobject.h>

#include <string>
#include <iostream>
#include <set>

#include "qresultstore.h"
#include "log.h"
#include "rclutil.h"

#include "pyrecoll.h"

using namespace std;

#if PY_MAJOR_VERSION >=3
#  define Py_TPFLAGS_HAVE_ITER 0
#else
#define PyLong_FromLong PyInt_FromLong 
#endif

struct recoll_QRSDocObject;

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    Rcl::QResultStore *store;
} recoll_QResultStoreObject;

static void 
QResultStore_dealloc(recoll_QResultStoreObject *self)
{
    LOGDEB1("QResultStore_dealloc.\n");
    delete self->store;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyDoc_STRVAR(qrs_doc_QResultStoreObject,
             "QResultStore()\n"
             "\n"
             "A QResultStore can efficiently store query result documents.\n"
    );

static int QResultStore_init(recoll_QResultStoreObject *self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("QResultStore_init\n");
    self->store = new Rcl::QResultStore();
    return 0;
}

PyDoc_STRVAR(
    qrs_doc_storeQuery,
    "storeQuery(query, fieldspec=[], isinc=False)\n"
    "\n"
    "Stores the results from the input query object, possibly "
    "excluding/including the specified fields.\n"
    );

static PyObject *
QResultStore_storeQuery(recoll_QResultStoreObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("QResultStore_storeQuery\n");
    static const char* kwlist[] = {"query", "fieldspec", "isinc", NULL};
    PyObject *q{nullptr};
    PyObject *fieldspec{nullptr};
    PyObject *isinco = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|OO", (char**)kwlist, 
                                     &recoll_QueryType, &q, &fieldspec, &isinco))
        return nullptr;

    recoll_QueryObject *query = (recoll_QueryObject*)q;
    if (nullptr == query->query) {
        PyErr_SetString(PyExc_ValueError,
                        "query not initialised (null query ?)");
        return nullptr;
    }
    bool isinc{false};
    if (nullptr != isinco && PyObject_IsTrue(isinco))
        isinc = true;
    
    std::set<std::string> fldspec;
    if (nullptr != fieldspec) {
        // fieldspec must be either single string or list of strings
        if (PyUnicode_Check(fieldspec)) {
            PyObject *utf8o = PyUnicode_AsUTF8String(fieldspec);
            if (nullptr == utf8o) {
                PyErr_SetString(PyExc_AttributeError,
                                "storeQuery: can't encode field name??");
                return nullptr;
            }
            fldspec.insert(PyBytes_AsString(utf8o));
            Py_DECREF(utf8o);
        } else if (PySequence_Check(fieldspec)) {
            for (Py_ssize_t i = 0; i < PySequence_Size(fieldspec); i++) {
                PyObject *utf8o =
                    PyUnicode_AsUTF8String(PySequence_GetItem(fieldspec, i));
                if (nullptr == utf8o) {
                    PyErr_SetString(PyExc_AttributeError,
                                    "storeQuery: can't encode field name??");
                    return nullptr;
                }
                fldspec.insert(PyBytes_AsString(utf8o));
                Py_DECREF(utf8o);
            }
        } else {
            PyErr_SetString(PyExc_TypeError,
                            "fieldspec arg must be str or sequence of str");
            return nullptr;
        }
    }
    self->store->storeQuery(*(query->query), fldspec, isinc);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(
    qrs_doc_getField,
    "getField(index, fieldname)\n"
    "\n"
    "Retrieve the value of field <fieldname> from result at index <index>.\n"
    );

static PyObject *QResultStore_getField(recoll_QResultStoreObject* self, PyObject *args)
{
    int index;
    const char *fieldname;
    if (!PyArg_ParseTuple(args, "is", &index, &fieldname)) {
        return nullptr;
    }
    const char *result = self->store->fieldValue(index, fieldname);
    if (nullptr == result) {
        Py_RETURN_NONE;
    } else {
        return PyBytes_FromString(result);
    }
}

static PyMethodDef QResultStore_methods[] = {
    {"storeQuery", (PyCFunction)QResultStore_storeQuery,
     METH_VARARGS|METH_KEYWORDS, qrs_doc_storeQuery},
    {"getField", (PyCFunction)QResultStore_getField,
     METH_VARARGS, qrs_doc_getField},

    {NULL}  /* Sentinel */
};

static Py_ssize_t QResultStore_Size(PyObject *o)
{
    return ((recoll_QResultStoreObject*)o)->store->getCount();
}

static PyObject* QResultStore_GetItem(PyObject *o, Py_ssize_t i)
{
    if (i < 0 || i >= ((recoll_QResultStoreObject*)o)->store->getCount()) {
        return nullptr;
    }
    PyObject *args = Py_BuildValue("Oi", o, i);
    auto res = PyObject_CallObject((PyObject *)&recoll_QRSDocType, args);
    Py_DECREF(args);
    return res;
}

static PySequenceMethods resultstore_as_sequence = {
    (lenfunc)QResultStore_Size, // sq_length
    (binaryfunc)0, // sq_concat
    (ssizeargfunc)0, // sq_repeat
    (ssizeargfunc)QResultStore_GetItem, // sq_item
    0, // was sq_slice
    (ssizeobjargproc)0, // sq_ass_item
    0, // was sq_ass_slice
    (objobjproc)0, // sq_contains
    (binaryfunc)0, // sq_inplace_concat
    (ssizeargfunc)0, // sq_inplace_repeat
};
        
PyTypeObject recoll_QResultStoreType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_recoll.QResultStore",
    .tp_basicsize = sizeof(recoll_QResultStoreObject),
    .tp_dealloc = (destructor)QResultStore_dealloc,
    .tp_as_sequence = &resultstore_as_sequence,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = qrs_doc_QResultStoreObject,
    .tp_methods = QResultStore_methods,
    .tp_init = (initproc)QResultStore_init,
    .tp_new = PyType_GenericNew,
};

////////////////////////////////////////////////////////////////////////
// QRSDoc iterator
typedef struct  recoll_QRSDocObject {
    PyObject_HEAD
    /* Type-specific fields go here. */
    recoll_QResultStoreObject *pystore;
    int index;
} recoll_QRSDocObject;

static void 
QRSDoc_dealloc(recoll_QRSDocObject *self)
{
    LOGDEB1("QRSDoc_dealloc\n");
    Py_DECREF(self->pystore);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyDoc_STRVAR(qrs_doc_QRSDocObject,
             "QRSDoc(resultstore, index)\n"
             "\n"
             "A QRSDoc gives access to one result from a qresultstore.\n"
    );

static int QRSDoc_init(recoll_QRSDocObject *self, PyObject *args, PyObject *kwargs)
{
    recoll_QResultStoreObject *pystore;
    int index;
    if (!PyArg_ParseTuple(args, "O!i", &recoll_QResultStoreType, &pystore, &index)) {
        return -1;
    }

    Py_INCREF(pystore);
    self->pystore = pystore;
    self->index = index;
    return 0;
}

static PyObject *QRSDoc_subscript(recoll_QRSDocObject *self, PyObject *key)
{
    if (self->pystore == 0) {
        PyErr_SetString(PyExc_AttributeError, "store??");
        return NULL;
    }
    string name;
    if (pys2cpps(key, name) < 0) {
        PyErr_SetString(PyExc_AttributeError, "name??");
        Py_RETURN_NONE;
    }

    const char *value = self->pystore->store->fieldValue(self->index, name);
   if (nullptr == value) {
        Py_RETURN_NONE;
    }
    PyObject *u = PyUnicode_Decode(value, strlen(value), "UTF-8", "surrogateescape");
    return u;
}

static PyMappingMethods qrsdoc_as_mapping = {
    (lenfunc)0, /*mp_length*/
    (binaryfunc)QRSDoc_subscript, /*mp_subscript*/
    (objobjargproc)0, /*mp_ass_subscript*/
};

PyTypeObject recoll_QRSDocType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_recoll.QRSDoc",
    .tp_basicsize = sizeof(recoll_QRSDocObject),
    .tp_dealloc = (destructor)QRSDoc_dealloc,
    .tp_as_mapping = &qrsdoc_as_mapping,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = qrs_doc_QRSDocObject,
    .tp_init = (initproc)QRSDoc_init,
    .tp_new = PyType_GenericNew,
};
