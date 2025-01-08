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

#include "rclinit.h"
#include "rclconfig.h"
#include "rcldb.h"
#include "searchdata.h"
#include "rclquery.h"
#include "pathut.h"
#include "rclutil.h"
#include "wasatorcl.h"
#include "log.h"
#include "plaintorich.h"
#include "hldata.h"
#include "smallut.h"
#include "idxstatus.h"
#include "rcldoc.h"

#include "pyrecoll.h"

using namespace std;

#if PY_MAJOR_VERSION >=3
#  define Py_TPFLAGS_HAVE_ITER 0
#else
#define PyLong_FromLong PyInt_FromLong 
#endif

static std::string stringfromobject(PyObject *pyobj)
{
    std::string out;
    if (PyUnicode_Check(pyobj)) {
        Py_ssize_t sz;
        const char *bytes = PyUnicode_AsUTF8AndSize(pyobj, &sz);
        out = std::string(bytes, sz);
    } else {
        if (!PyBytes_Check(pyobj)) {
            PyErr_SetString(PyExc_AttributeError, "Input must be str or bytes");
            return 0;
        }
        out = std::string(PyBytes_AsString(pyobj), PyBytes_Size(pyobj));
    }
    return out;
}

//////////////////////////////////////////////////////////////////////
/// SEARCHDATA SearchData code
typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    std::shared_ptr<Rcl::SearchData> sd;
} recoll_SearchDataObject;

static void 
SearchData_dealloc(recoll_SearchDataObject *self)
{
    LOGDEB("SearchData_dealloc. Releasing. Count before: " <<
           self->sd.use_count() << "\n");
    self->sd.reset();
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyDoc_STRVAR(doc_SearchDataObject,
             "SearchData([type=AND|OR], [stemlang=somelanguage|null])\n"
             "\n"
             "A SearchData object describes a query. It has a number of global\n"
             "parameters and a chain of search clauses.\n"
    );

static int
SearchData_init(recoll_SearchDataObject *self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("SearchData_init\n");
    static const char* kwlist[] = {"type", "stemlang", NULL};
    char *stp = 0;
    char *steml = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sz", (char**)kwlist, &stp, &steml))
        return -1;
    Rcl::SClType tp = Rcl::SCLT_AND;

    if (stp && strcasecmp(stp, "or")) {
        tp = Rcl::SCLT_OR;
    }
    string stemlang;
    if (steml) {
        stemlang = steml;
    } else {
        stemlang = "english";
    }
    self->sd = std::make_shared<Rcl::SearchData>(tp, stemlang);
    return 0;
}

/* Note: addclause necessite And/Or vient du fait que le string peut avoir
   plusieurs mots. A transferer dans l'i/f Python ou pas ? */
PyDoc_STRVAR(doc_addclause,
             "addclause(type='and'|'or'|'filename'|'phrase'|'near'|'path'|'sub',\n"
             "          qstring=string, slack=int, field=string, stemming=1|0,\n"
             "          subSearch=SearchData, exclude=0|1, anchorstart=0|1, anchorend=0|1,\n"
             "          casesens=0|1, diacsens=0|1)\n"
             "Adds a simple clause to the SearchData And/Or chain, or a subquery\n"
             "defined by another SearchData object\n"
    );

/* Forward declaration only, definition needs recoll_searchDataType */
static PyObject *
SearchData_addclause(recoll_SearchDataObject* self, PyObject *args, 
                     PyObject *kwargs);

static PyMethodDef SearchData_methods[] = {
    {"addclause", (PyCFunction)SearchData_addclause, METH_VARARGS|METH_KEYWORDS, doc_addclause},
    {NULL}  /* Sentinel */
};

static PyTypeObject recoll_SearchDataType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_recoll.SearchData",
    .tp_basicsize = sizeof(recoll_SearchDataObject),
    .tp_dealloc = (destructor)SearchData_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = doc_SearchDataObject,
    .tp_methods = SearchData_methods,
    .tp_init = (initproc)SearchData_init,
    .tp_new = PyType_GenericNew,
};

static PyObject *
SearchData_addclause(recoll_SearchDataObject* self, PyObject *args, 
                     PyObject *kwargs)
{
    LOGDEB0("SearchData_addclause\n");
    if (!self->sd) {
        LOGERR("SearchData_addclause: not init??\n");
        PyErr_SetString(PyExc_AttributeError, "sd");
        return 0;
    }
    static const char *kwlist[] = {"type", "qstring", "slack", "field", 
                                   "stemming", "subsearch", 
                                   "exclude", "anchorstart", "anchorend",
                                   "casesens", "diacsens",
                                   NULL};
    char *tp = 0;
    char *qs = 0; // needs freeing
    int slack = 0;
    char *fld = 0; // needs freeing
    PyObject  *dostem = 0;
    recoll_SearchDataObject *sub = 0;
    PyObject *exclude = 0;
    PyObject *anchorstart = 0;
    PyObject *anchorend = 0;
    PyObject *casesens = 0;
    PyObject *diacsens = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ses|iesOO!OOOOO", 
                                     (char**)kwlist,
                                     &tp, "utf-8", &qs, &slack,
                                     "utf-8", &fld, &dostem,
                                     &recoll_SearchDataType, &sub,
                                     &exclude, &anchorstart, &anchorend,
                                     &casesens, &diacsens
            ))
        return 0;

    Rcl::SearchDataClause *cl = 0;

    switch (tp[0]) {
    case 'a':
    case 'A':
        if (strcasecmp(tp, "and"))
            goto defaultcase;
        cl = new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, qs, fld ? fld : "");
        break;
    case 'f':
    case 'F':
        if (strcasecmp(tp, "filename"))
            goto defaultcase;
        cl = new Rcl::SearchDataClauseFilename(qs);
        break;
    case 'o':
    case 'O':
        if (strcasecmp(tp, "or"))
            goto defaultcase;
        cl = new Rcl::SearchDataClauseSimple(Rcl::SCLT_OR, qs, fld ? fld : "");
        break;
    case 'n':
    case 'N':
        if (strcasecmp(tp, "near"))
            goto defaultcase;
        cl = new Rcl::SearchDataClauseDist(Rcl::SCLT_NEAR, qs, slack, fld ? fld : "");
        break;
    case 'p':
    case 'P':
        if (!strcasecmp(tp, "phrase")) {
            cl = new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, qs, slack, fld ? fld : "");
        } else if (!strcasecmp(tp, "path")) {
            cl = new Rcl::SearchDataClausePath(qs);
        } else {
            goto defaultcase;
        }
        break;
    case 's':
    case 'S':
        if (strcasecmp(tp, "sub"))
            goto defaultcase;
        cl = new Rcl::SearchDataClauseSub(sub->sd);
        break;
    defaultcase:
    default:
        PyErr_SetString(PyExc_AttributeError, "Bad tp arg");
        return 0;
    }
    PyMem_Free(qs);
    PyMem_Free(fld);

    if (dostem != 0 && !PyObject_IsTrue(dostem)) {
        cl->addModifier(Rcl::SearchDataClause::SDCM_NOSTEMMING);
    }
    if (exclude != 0 && !PyObject_IsTrue(exclude)) {
        cl->setexclude(true);
    }
    if (anchorstart && PyObject_IsTrue(anchorstart)) {
        cl->addModifier(Rcl::SearchDataClause::SDCM_ANCHORSTART);
    }
    if (anchorend && PyObject_IsTrue(anchorend)) {
        cl->addModifier(Rcl::SearchDataClause::SDCM_ANCHOREND);
    }
    if (casesens && PyObject_IsTrue(casesens)) {
        cl->addModifier(Rcl::SearchDataClause::SDCM_CASESENS);
    }
    if (diacsens && PyObject_IsTrue(diacsens)) {
        cl->addModifier(Rcl::SearchDataClause::SDCM_DIACSENS);
    }
    self->sd->addClause(cl);
    Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////
///// DOC Doc code

static void 
Doc_dealloc(recoll_DocObject *self)
{
    LOGDEB0("Doc_dealloc\n");
    deleteZ(self->doc);
    self->rcldb.reset();
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int
Doc_init(recoll_DocObject *self, PyObject *, PyObject *)
{
    LOGDEB0("Doc_init\n");
    self->doc = new Rcl::Doc;
    return 0;
}

PyDoc_STRVAR(
    doc_Doc_getbinurl,
    "getbinurl(none) -> binary url\n"
    "\n"
    "Returns an URL with a path part which is a as bit for bit copy of the \n"
    "file system path, without encoding\n"
    );

static PyObject *
Doc_getbinurl(recoll_DocObject *self)
{
    LOGDEB0("Doc_getbinurl\n");
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc is NULL");
        Py_RETURN_NONE;
    }
    return PyBytes_FromStringAndSize(self->doc->url.c_str(), self->doc->url.size());
}

PyDoc_STRVAR(
    doc_Doc_setbinurl,
    "setbinurl(url) -> binary url\n"
    "\n"
    "Set the URL from binary path like file://may/contain/unencodable/bytes\n"
    );

static PyObject *
Doc_setbinurl(recoll_DocObject *self, PyObject *value)
{
    LOGDEB0("Doc_setbinurl\n");
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc??");
        return 0;
    }
    if (!self->rcldb || !self->rcldb->getConf()->ok()) {
        PyErr_SetString(PyExc_AttributeError, "Configuration not initialized");
        return 0;
    }
    if (PyByteArray_Check(value)) {
        self->doc->url = string(PyByteArray_AsString(value), PyByteArray_Size(value));
    } else if (PyBytes_Check(value)) {
        self->doc->url = string(PyBytes_AsString(value), PyBytes_Size(value));
    } else {
        PyErr_SetString(PyExc_TypeError, "setbinurl needs bytearray or bytes argument");
        return 0;
    }        

    printableUrl(self->rcldb->getConf()->getDefCharset(),
                 self->doc->url, self->doc->meta[Rcl::Doc::keyurl]);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(doc_Doc_keys,
             "keys() -> list of doc object keys (attribute names)\n"
    );
static PyObject *
Doc_keys(recoll_DocObject *self)
{
    LOGDEB0("Doc_keys\n");
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }

    PyObject *pkeys = PyList_New(0);
    if (!pkeys)
        return 0;
    for (const auto& entry : self->doc->meta) {
        PyList_Append(
            pkeys, PyUnicode_Decode(entry.first.c_str(), entry.first.size(), "UTF-8", "replace"));
    }
    return pkeys;
}

PyDoc_STRVAR(doc_Doc_items,
             "items() -> dictionary of doc object keys/values\n"
    );
static PyObject *
Doc_items(recoll_DocObject *self)
{
    LOGDEB0("Doc_items\n");
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }

    PyObject *pdict = PyDict_New();
    if (!pdict)
        return 0;
    for (const auto& entry : self->doc->meta) {
        PyDict_SetItem(pdict, 
                       PyUnicode_Decode(
                           entry.first.c_str(), entry.first.size(), "UTF-8", "replace"),
                       PyUnicode_Decode(
                           entry.second.c_str(), entry.second.size(), "UTF-8", "replace"));
    }
    return pdict;
}

static bool idocget(recoll_DocObject *self, const string& key, string& value)
{
    LOGDEB1("idocget: key [" << key << "]\n");
    switch (key.at(0)) {
    case 'u':
        if (!key.compare(Rcl::Doc::keyurl)) {
            value = self->doc->url;
            return true;
        }
        break;
    case 'f':
        if (!key.compare(Rcl::Doc::keyfs)) {
            value = self->doc->fbytes;
            return true;
        } else if (!key.compare(Rcl::Doc::keyfmt)) {
            value = self->doc->fmtime;
            return true;
        }
        break;
    case 'd':
        if (!key.compare(Rcl::Doc::keyds)) {
            value = self->doc->dbytes;
            return true;
        } else if (!key.compare(Rcl::Doc::keydmt)) {
            value = self->doc->dmtime;
            return true;
        }
        break;
    case 'i':
        if (!key.compare(Rcl::Doc::keyipt)) {
            value = self->doc->ipath;
            return true;
        }
        break;
    case 'm':
        if (!key.compare(Rcl::Doc::keytp)) {
            value = self->doc->mimetype;
            return true;
        } else if (!key.compare(Rcl::Doc::keymt)) {
            value = self->doc->dmtime.empty() ? self->doc->fmtime : self->doc->dmtime;
            return true;
        }
        break;
    case 'o':
        if (!key.compare(Rcl::Doc::keyoc)) {
            value = self->doc->origcharset;
            return true;
        }
        break;
    case 'r':
        if (!key.compare(Rcl::Doc::keyudi)) {
            value = self->rcldb->fetchUdi(*self->doc);
            return true;
        }        
    case 's':
        if (!key.compare(Rcl::Doc::keysig)) {
            value = self->doc->sig;
            return true;
        } else     if (!key.compare(Rcl::Doc::keysz)) {
            value = self->doc->dbytes.empty() ? self->doc->fbytes : self->doc->dbytes;
            return true;
        }
        break;
    case 't':
        if (!key.compare("text")) {
            value = self->doc->text;
            return true;
        }
        break;
    case 'x':
        if (!key.compare("xdocid")) {
            ulltodecstr(self->doc->xdocid, value);
            return true;
        }
        break;
    }

    if (self->doc->getmeta(key, 0)) {
        value = self->doc->meta[key];
        return true;
    }
    return false;
}

PyDoc_STRVAR(doc_Doc_get,
             "get(key) -> value\n"
             "Retrieve the named doc attribute\n"
    );
static PyObject *
Doc_get(recoll_DocObject *self, PyObject *args)
{
    LOGDEB1("Doc_get\n");
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc??");
        return 0;
    }
    char *sutf8 = 0; // needs freeing
    if (!PyArg_ParseTuple(args, "es:Doc_get", "utf-8", &sutf8)) {
        return 0;
    }
    string key(sutf8);
    PyMem_Free(sutf8);

    string value;
    if (idocget(self, key, value)) {
        return PyUnicode_Decode(value.c_str(), value.size(), "UTF-8","replace");
    }

    Py_RETURN_NONE;
}

static PyMethodDef Doc_methods[] = {
    {"getbinurl", (PyCFunction)Doc_getbinurl, METH_NOARGS, doc_Doc_getbinurl},
    {"setbinurl", (PyCFunction)Doc_setbinurl, METH_O, doc_Doc_setbinurl},
    {"keys", (PyCFunction)Doc_keys, METH_NOARGS, doc_Doc_keys},
    {"items", (PyCFunction)Doc_items, METH_NOARGS, doc_Doc_items},
    {"get", (PyCFunction)Doc_get, METH_VARARGS, doc_Doc_get},
        
    {NULL}  /* Sentinel */
};

int pys2cpps(PyObject *pyval, std::string& out)
{
    if (PyUnicode_Check(pyval)) {
        PyObject* utf8o = PyUnicode_AsUTF8String(pyval);
        if (utf8o == 0) {
            return -1;
        }
        out = PyBytes_AsString(utf8o);
        Py_DECREF(utf8o);
    }  else if (PyBytes_Check(pyval)) {
        out = PyBytes_AsString(pyval);
    } else {
        return -1;
    }
    return 0;
}

// Note that this returns None if the attribute is not found instead of raising
// an exception as would be standard. We don't change it to keep existing code
// working.
static PyObject *
Doc_getattro(recoll_DocObject *self, PyObject *nameobj)
{
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }
    if (!self->rcldb || !self->rcldb->getConf()->ok()) {
        PyErr_SetString(PyExc_AttributeError, "Configuration not initialized");
        return 0;
    }

    PyObject *meth = PyObject_GenericGetAttr((PyObject*)self, nameobj);
    if (meth) {
        return meth;
    }
    PyErr_Clear();
    
    string name;
    if (pys2cpps(nameobj, name) < 0) {
        PyErr_SetString(PyExc_AttributeError, "name not unicode nor string??");
        Py_RETURN_NONE;
    }

    string key = self->rcldb->getConf()->fieldQCanon(name);
    string value;
    if (idocget(self, key, value)) {
        LOGDEB1("Doc_getattro: [" << key << "] -> [" << value << "]\n");
        // Return a python unicode object
        return PyUnicode_Decode(value.c_str(), value.size(), "utf-8","replace");
    }
    
    Py_RETURN_NONE;
}

static int
Doc_setattro(recoll_DocObject *self, PyObject *nameobj, PyObject *value)
{
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc??");
        return -1;
    }
    if (!self->rcldb || !self->rcldb->getConf()->ok()) {
        PyErr_SetString(PyExc_AttributeError, "Configuration not initialized");
        return -1;
    }
    string name;
    if (pys2cpps(nameobj, name) < 0) {
        PyErr_SetString(PyExc_AttributeError, "name not unicode nor string??");
        return -1;
    }

    string uvalue;
    if (pys2cpps(value, uvalue) < 0) {
        PyErr_SetString(PyExc_AttributeError, "value neither bytes nor str");
        return -1;
    }

    string key = self->rcldb->getConf()->fieldQCanon(name);

    LOGDEB0("Doc_setattr: doc " << self->doc << " [" << key << "] (" << name <<
            ") -> [" << uvalue << "]\n");

    // Note that some attributes are set both as struct fields and
    // meta members, keep compat with movedocfields() used when
    // fetching from query.
    self->doc->meta[key] = uvalue;
    switch (key.at(0)) {
    case 't':
        if (key == "text") {
            self->doc->text.swap(uvalue);
        }
        break;
    case 'u':
        if (key == Rcl::Doc::keyurl) {
            self->doc->url.swap(uvalue);
            printableUrl(self->rcldb->getConf()->getDefCharset(), self->doc->url, 
                         self->doc->meta[Rcl::Doc::keyurl]);
        }
        break;
    case 'f':
        if (key == Rcl::Doc::keyfs) {
            self->doc->fbytes.swap(uvalue);
            self->doc->meta[Rcl::Doc::keyfs] = self->doc->fbytes;
        } else if (key == Rcl::Doc::keyfmt) {
            self->doc->fmtime.swap(uvalue);
        }
        break;
    case 'd':
        if (key == Rcl::Doc::keyds) {
            self->doc->dbytes.swap(uvalue);
            self->doc->meta[Rcl::Doc::keyds] = self->doc->dbytes;
        } else if (key == Rcl::Doc::keydmt) {
            self->doc->dmtime.swap(uvalue);
        }
        break;
    case 'i':
        if (key == Rcl::Doc::keyipt) {
            self->doc->ipath.swap(uvalue);
            self->doc->meta[Rcl::Doc::keyipt] = self->doc->ipath;
        }
        break;
    case 'm':
        if (key == Rcl::Doc::keytp) {
            self->doc->mimetype.swap(uvalue);
            self->doc->meta[Rcl::Doc::keytp] = self->doc->mimetype;
        } else if (key == Rcl::Doc::keymt) {
            self->doc->dmtime.swap(uvalue);
        }
        break;
    case 'o':
        if (key == Rcl::Doc::keyoc) {
            self->doc->origcharset.swap(uvalue);
        }
        break;
    case 's':
        if (key == Rcl::Doc::keysig) {
            self->doc->sig.swap(uvalue);
        } else if (key == Rcl::Doc::keysz) {
            self->doc->dbytes.swap(uvalue);
        }
        break;
    }
    return 0;
}

static Py_ssize_t
Doc_length(recoll_DocObject *self)
{
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc??");
        return -1;
    }
    return self->doc->meta.size();
}

static PyObject *
Doc_subscript(recoll_DocObject *self, PyObject *key)
{
    // Can't just return getattro because this first checks for a method name
    if (self->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "doc??");
        return NULL;
    }
    if (!self->rcldb || !self->rcldb->getConf()->ok()) {
        PyErr_SetString(PyExc_AttributeError, "Configuration not initialized");
        return NULL;
    }
    string name;
    if (pys2cpps(key, name) < 0) {
        PyErr_SetString(PyExc_AttributeError, "key not unicode nor string??");
        Py_RETURN_NONE;
    }

    string skey = self->rcldb->getConf()->fieldQCanon(name);
    string value;
    if (idocget(self, skey, value)) {
        return PyUnicode_Decode(value.c_str(), value.size(), "UTF-8", "backslashreplace");
    }
    Py_RETURN_NONE;
}

static int
Doc_ass_subscript(recoll_DocObject *self, PyObject *key, PyObject *val)
{
    return Doc_setattro(self, key, val);
}

static PyMappingMethods doc_as_mapping = {
    (lenfunc)Doc_length, /*mp_length*/
    (binaryfunc)Doc_subscript, /*mp_subscript*/
    (objobjargproc)Doc_ass_subscript, /*mp_ass_subscript*/
};


PyDoc_STRVAR(
    doc_DocObject,
    "Doc()\n"
    "\n"
    "A Doc object contains index data for a given document.\n"
    "The data is extracted from the index when searching, or set by the\n"
    "indexer program when updating. The Doc object has no useful methods but\n"
    "many attributes to be read or set by its user. It matches exactly the\n"
    "Rcl::Doc c++ object. Some of the attributes are predefined, but, \n"
    "especially when indexing, others can be set, the name of which will be\n"
    "processed as field names by the indexing configuration.\n"
    "Inputs can be specified as unicode or strings.\n"
    "Outputs are unicode objects.\n"
    "All dates are specified as unix timestamps, printed as strings\n"
    "Predefined attributes (index/query/both):\n"
    " text (index): document plain text\n"
    " url (both)\n"
    " fbytes (both) optional) file size in bytes\n"
    " filename (both)\n"
    " fmtime (both) optional file modification date. Unix time printed \n"
    "    as string\n"
    " dbytes (both) document text bytes\n"
    " dmtime (both) document creation/modification date\n"
    " ipath (both) value private to the app.: internal access path\n"
    "    inside file\n"
    " mtype (both) mime type for original document\n"
    " mtime (query) dmtime if set else fmtime\n"
    " origcharset (both) charset the text was converted from\n"
    " size (query) dbytes if set, else fbytes\n"
    " sig (both) app-defined file modification signature. \n"
    "    For up to date checks\n"
    " relevancyrating (query)\n"
    " abstract (both)\n"
    " author (both)\n"
    " title (both)\n"
    " keywords (both)\n"
    );

PyTypeObject recoll_DocType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_recoll.Doc",
    .tp_basicsize = sizeof(recoll_DocObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Doc_dealloc,
    .tp_as_mapping = &doc_as_mapping,
    .tp_getattro = (getattrofunc)Doc_getattro,
    .tp_setattro = (setattrofunc)Doc_setattro,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = doc_DocObject,
    .tp_methods = Doc_methods,
    .tp_init = (initproc)Doc_init,
    .tp_new = PyType_GenericNew,
};


//////////////////////////////////////////////////////
/// QUERY Query object 

typedef struct recoll_DbObject {
    PyObject_HEAD
    /* Type-specific fields go here. */
    std::shared_ptr<Rcl::Db> rcldb;
} recoll_DbObject;

PyDoc_STRVAR(doc_Query_close,
             "close(). Deallocate query. Object is unusable after the call."
    );
static PyObject *
Query_close(recoll_QueryObject *self)
{
    LOGDEB("Query_close\n");
    if (self->query) {
        deleteZ(self->query);
    }
    deleteZ(self->sortfield);
    if (self->connection) {
        Py_DECREF(self->connection);
        self->connection = 0;
    }
    Py_RETURN_NONE;
}

static void 
Query_dealloc(recoll_QueryObject *self)
{
    LOGDEB("Query_dealloc\n");
    PyObject *ret = Query_close(self);
    Py_DECREF(ret);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

// Query_init creates an unusable object. The only way to create a valid Query Object is through
// db_query(). (or we'd need to add a Db parameter to the Query object creation method)
static int
Query_init(recoll_QueryObject *self, PyObject *, PyObject *)
{
    LOGDEB0("Query_init\n");
    self->query = 0;
    self->next = -1;
    self->rowcount = -1;
    self->sortfield = new string;
    self->ascending = 1;
    self->arraysize = 1;
    self->connection = 0;
    self->fetchtext = false;
    return 0;
}

static PyObject *
Query_iter(PyObject *self)
{
    Py_INCREF(self);
    return self;
}

PyDoc_STRVAR(doc_Query_sortby,
             "sortby(field=fieldname, ascending=True)\n"
             "Sort results by 'fieldname', in ascending or descending order.\n"
             "Only one field can be used, no subsorts for now.\n"
             "Must be called before executing the search\n"
    );

static PyObject *
Query_sortby(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_sortby\n");
    static const char *kwlist[] = {"field", "ascending", NULL};
    char *sfield = 0;
    PyObject *ascobj = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", (char**)kwlist, &sfield, &ascobj))
        return 0;

    if (sfield) {
        self->sortfield->assign(sfield);
    } else {
        self->sortfield->clear();
    }
    if (ascobj == 0) {
        self->ascending = true;
    } else {
        self->ascending = PyObject_IsTrue(ascobj);
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(doc_Query_execute,
             "execute(query_string, stemming=1|0, stemlang=\"stemming language\", "
             "fetchtext=False)\n"
             "\n"
             "Starts a search for query_string, a Recoll search language string\n"
             "(mostly Xesam-compatible).\n"
             "The query can be a simple list of terms (and'ed by default), or more\n"
             "complicated with field specs etc. See the Recoll manual.\n"
    );

static PyObject *
Query_execute(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_execute\n");
    static const char *kwlist[] = {"query_string", "stemming", "stemlang",
                                   "fetchtext", "collapseduplicates", NULL};
    char *sutf8 = 0; // needs freeing
    char *sstemlang = 0;
    PyObject *dostemobj = 0;
    PyObject *fetchtextobj = 0;
    PyObject *collapseobj = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "es|OesOO:Query_execute", 
                                     (char**)kwlist, "utf-8", &sutf8,
                                     &dostemobj, "utf-8", &sstemlang,
                                     &fetchtextobj, &collapseobj)) {
        return 0;
    }

    bool dostem{true};
    if (dostemobj != 0 && !PyObject_IsTrue(dostemobj))
        dostem = false;
    if (fetchtextobj != 0 && PyObject_IsTrue(fetchtextobj)) {
        self->fetchtext = true;
    } else {
        self->fetchtext = false;
    }

    string utf8(sutf8);
    PyMem_Free(sutf8);
    string stemlang("english");
    if (sstemlang) {
        stemlang.assign(sstemlang);
        PyMem_Free(sstemlang);
    }

    LOGDEB0("Query_execute: [" << utf8 << "] dostem " << dostem <<
            " stemlang [" << stemlang << "]\n");

    if (self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }

    if (collapseobj != 0 && PyObject_IsTrue(collapseobj)) {
        self->query->setCollapseDuplicates(true);
    } else {
        self->query->setCollapseDuplicates(false);
    }

    // The searchdata does not keep a ref to the rclconfig, it's just used during the call. So ok to
    // use the raw pointer.
    string reason;
    std::shared_ptr<Rcl::SearchData> rq = wasaStringToRcl(
        self->connection->rcldb->getConf(), dostem ? stemlang : "", utf8, reason);

    if (!rq) {
        PyErr_SetString(PyExc_ValueError, reason.c_str());
        return 0;
    }
    
    self->query->setSortBy(*self->sortfield, self->ascending);
    self->query->setQuery(rq);
    int cnt = self->query->getResCnt();
    self->next = 0;
    self->rowcount = cnt;
    return Py_BuildValue("i", cnt);
}

PyDoc_STRVAR(doc_Query_executesd,
             "executesd(SearchData, fetchtext=False)\n"
             "\n"
             "Starts a search for the query defined by the SearchData object.\n"
    );

static PyObject *
Query_executesd(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_executeSD\n");
    static const char *kwlist[] = {"searchdata", "fetchtext",
                                   "collapseduplicates", NULL};
    recoll_SearchDataObject *pysd = 0;
    PyObject *fetchtextobj = 0;
    PyObject *collapseobj = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|OO:Query_execute", 
                                     (char **)kwlist, &recoll_SearchDataType,
                                     &pysd, &fetchtextobj, &collapseobj)) {
        return 0;
    }
    if (pysd == 0 || self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    if (fetchtextobj != 0 && PyObject_IsTrue(fetchtextobj)) {
        self->fetchtext = true;
    } else {
        self->fetchtext = false;
    }
    if (collapseobj != 0 && PyObject_IsTrue(collapseobj)) {
        self->query->setCollapseDuplicates(true);
    } else {
        self->query->setCollapseDuplicates(false);
    }
        
    self->query->setSortBy(*self->sortfield, self->ascending);
    self->query->setQuery(pysd->sd);
    int cnt = self->query->getResCnt();
    self->next = 0;
    self->rowcount = cnt;
    return Py_BuildValue("i", cnt);
}

// Move some data from the dedicated fields to the meta array to make fetching attributes
// easier. Needed because we only use the meta array when enumerating keys. Also for url which is
// also formatted.  But note that some fields are not copied, and are only reachable (through
// doc_Get, as doc[fldname]) if one knows their name (e.g. xdocid).
static void movedocfields(const RclConfig *rclconfig, Rcl::Doc *doc)
{
    printableUrl(rclconfig->getDefCharset(), doc->url, doc->meta[Rcl::Doc::keyurl]);
    doc->meta[Rcl::Doc::keytp] = doc->mimetype;
    doc->meta[Rcl::Doc::keyipt] = doc->ipath;
    doc->meta[Rcl::Doc::keyfs] = doc->fbytes;
    doc->meta[Rcl::Doc::keyds] = doc->dbytes;
}

static PyObject *
Query_iternext(PyObject *_self)
{
    LOGDEB0("Query_iternext\n");
    recoll_QueryObject* self = (recoll_QueryObject*)_self;

    if (self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    int cnt = self->query->getResCnt();
    if (cnt <= 0 || self->next < 0) {
        // This happens if there are no results and is not an error
        return 0;
    }
    recoll_DocObject *result =
        (recoll_DocObject *)PyObject_CallObject((PyObject *)&recoll_DocType, 0);
    if (!result) {
        PyErr_SetString(PyExc_EnvironmentError, "doc create failed");
        return 0;
    }
    result->rcldb = self->connection->rcldb;
    // We used to check against rowcount here, but this was wrong: xapian result count estimate are
    // sometimes wrong, we must go on fetching until we fail
    if (!self->query->getDoc(self->next, *result->doc, self->fetchtext)) {
        return 0;
    }
    self->next++;

    movedocfields(self->connection->rcldb->getConf(), result->doc);
    return (PyObject *)result;
}

PyDoc_STRVAR(doc_Query_fetchone,
             "fetchone(None) -> Doc\n"
             "\n"
             "Fetches the next Doc object in the current search results.\n"
    );
static PyObject *
Query_fetchone(PyObject *_self)
{
    LOGDEB0("Query_fetchone/next\n");

    recoll_DocObject *result = (recoll_DocObject *)Query_iternext(_self);
    if (!result) {
        Py_RETURN_NONE;
    }
    return (PyObject *)result;
}

PyDoc_STRVAR(doc_Query_fetchmany,
             "fetchmany([size=query.arraysize]) -> Doc list\n"
             "\n"
             "Fetches the next Doc objects in the current search results.\n"
    );
static PyObject *
Query_fetchmany(PyObject* _self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_fetchmany\n");
    recoll_QueryObject* self = (recoll_QueryObject*)_self;
    static const char *kwlist[] = {"size", NULL};
    int size = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", (char**)kwlist, &size))
        return 0;

    if (size == 0)
        size = self->arraysize;

    PyObject *reslist = PyList_New(0);
    for (int i = 0; i < size; i++) {
        recoll_DocObject *docobj = (recoll_DocObject *)Query_iternext(_self);
        if (!docobj) {
            break;
        }
        PyList_Append(reslist,  (PyObject*)docobj);
        Py_DECREF(docobj);
    }

    if (PyErr_Occurred()) {
        Py_DECREF(reslist);
        return NULL;
    } else {
        return reslist;
    }
}


PyDoc_STRVAR(doc_Query_scroll,
             "scroll(value, [, mode='relative'/'absolute' ]) -> new int position\n"
             "\n"
             "Adjusts the position in the current result set.\n"
    );
static PyObject *
Query_scroll(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_scroll\n");
    static const char *kwlist[] = {"position", "mode", NULL};
    int pos = 0;
    char *smode = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|s", (char**)kwlist, &pos, &smode))
        return 0;

    bool isrelative = 1;
    if (smode != 0) {
        if (!strcasecmp(smode, "relative")) {
            isrelative = 1;
        } else if (!strcasecmp(smode, "absolute")) {
            isrelative = 0;
        } else {
            PyErr_SetString(PyExc_ValueError, "bad mode value");
            return 0;
        }
    } 

    if (self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "null query");
        return 0;
    }
    int newpos = isrelative ? self->next + pos : pos;
    if (newpos < 0 || newpos >= self->rowcount) {
        PyErr_SetString(PyExc_IndexError, "position out of range");
        return 0;
    }
    self->next = newpos;
    return Py_BuildValue("i", newpos);
}

PyDoc_STRVAR(doc_Query_highlight,
             "highlight(text, ishtml = 0/1, methods = object))\n"
             "Will insert <span \"class=rclmatch\"></span> tags around the match areas\n"
             "in the input text and return the modified text\n"
             "ishtml can be set to indicate that the input text is html and html special\n"
             " characters should not be escaped\n"
             "methods if set should be an object with methods startMatch(i) and endMatch()\n"
             "  which will be called for each match and should return a begin and end tag\n"
    );

class PyPlainToRich: public PlainToRich {
public:
    PyPlainToRich() {}
    PyPlainToRich(PyObject *methods, bool eolbr = false,
                  // Do not use default markers if methods is nullptr
                  bool nohl = false)
        : m_methods(methods) {
        m_eolbr = eolbr;
        m_nohl = nohl;
    }
    virtual ~PyPlainToRich() = default;
    virtual string startMatch(unsigned int idx) {
        // if nohl is set or methods was explicitely set to None, do nothing.
        if (m_nohl || m_methods == Py_None) {
            return "";
        }

        // Use either user-defined methods or our default
        PyObject *res =  0;
        if (m_methods && PyObject_HasAttrString(m_methods, (char *)"startMatch")) {
            res = PyObject_CallMethod(m_methods, (char *)"startMatch", (char *)"(i)", idx);
        }
        if (res == 0) {
            return "<span class=\"rclmatch\">";
        }
        PyObject *res1 = res;
        if (PyUnicode_Check(res))
            res1 = PyUnicode_AsUTF8String(res);
        return PyBytes_AsString(res1);
    } 

    virtual string endMatch() {
        if (m_nohl || m_methods == Py_None) {
            return "";
        }
        PyObject *res =  0;
        if (m_methods && PyObject_HasAttrString(m_methods, (char *)"endMatch"))
            res = PyObject_CallMethod(m_methods, (char *)"endMatch", 0);
        if (res == 0) {
            return "</span>";
        }
        PyObject *res1 = res;
        if (PyUnicode_Check(res))
            res1 = PyUnicode_AsUTF8String(res);
        return PyBytes_AsString(res1);
    }

    PyObject *m_methods{nullptr};
    bool m_nohl{false};
};

static PyObject *
Query_highlight(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_highlight\n");
    static const char *kwlist[] = {"text", "ishtml", "eolbr", "methods", NULL};
    char *sutf8 = 0; // needs freeing
    int ishtml = 0;
    PyObject *ishtmlobj = 0;
    int eolbr = 1;
    PyObject *eolbrobj = 0;
    PyObject *methods = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "es|OOO:Query_highlight",
                                     (char**)kwlist, 
                                     "utf-8", &sutf8,
                                     &ishtmlobj,
                                     &eolbrobj,
                                     &methods)) {
        return 0;
    }
    string utf8(sutf8);
    PyMem_Free(sutf8);
    if (ishtmlobj && PyObject_IsTrue(ishtmlobj))
        ishtml = 1;
    if (eolbrobj && !PyObject_IsTrue(eolbrobj))
        eolbr = 0;
    LOGDEB0("Query_highlight: ishtml " << ishtml << "\n");

    if (self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }

    std::shared_ptr<Rcl::SearchData> sd = self->query->getSD();
    if (!sd) {
        PyErr_SetString(PyExc_ValueError, "Query not initialized");
        return 0;
    }
    HighlightData hldata;
    sd->getTerms(hldata);
    PyPlainToRich hler(methods, eolbr);
    hler.set_inputhtml(ishtml);
    list<string> out;
    hler.plaintorich(utf8, out, hldata, 5000000);
    if (out.empty()) {
        PyErr_SetString(PyExc_ValueError, "Plaintorich failed");
        return 0;
    }
    // cf python manual:The bytes will be interpreted as being UTF-8 encoded.
    PyObject* unicode = PyUnicode_FromStringAndSize(out.begin()->c_str(), out.begin()->size());
    // We used to return a copy of the unicode object. Can't see why any more
    return unicode;
}

// Helper shared by db.makeDocAbstract() and query.makeDocAbstract()
static std::string makedocabstract(Rcl::Query *query, const Rcl::Doc& doc, PyObject *hlmethods,
                                   bool nohl = false)
{
    string abstract;
    PyPlainToRich hler(hlmethods, false, nohl);
    hler.set_inputhtml(0);
    vector<string> vabs;
    query->makeDocAbstract(doc, &hler, vabs);
    for (unsigned int i = 0; i < vabs.size(); i++) {
        if (vabs[i].empty())
            continue;
        abstract += vabs[i];
        abstract += "...";
    }
    return abstract;
}

PyDoc_STRVAR(doc_Query_makedocabstract,
             "makedocabstract(doc, methods = object, nohl=False))\n"
             "Will create a snippets abstract for doc by selecting text around the match\n"
             " terms\n"
             "If methods is set, will also perform highlighting. See the highlight method\n"
             "If methods is not set and nohl is true, it will disable the default insertion of "
             "match regions\n"
    );

static PyObject *Query_makedocabstract(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_makeDocAbstract\n");
    static const char *kwlist[] = {"doc", "methods", "nohl", NULL};
    recoll_DocObject *pydoc = 0;
    PyObject *hlmethods = 0;
    PyObject *nohlobj = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|OO:Query_makeDocAbstract", (char **)kwlist,
                                     &recoll_DocType, &pydoc, &hlmethods, &nohlobj)) {
        return 0;
    }
    bool nohl{false};
    if (nohlobj != 0 && PyObject_IsTrue(nohlobj))
        nohl = true;

    if (pydoc->doc == 0) {
        LOGERR("Query_makeDocAbstract: doc not found " << pydoc->doc << "\n");
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }
    if (nullptr == self->query) {
        LOGERR("Query_makeDocAbstract: query not found " << self->query<< "\n");
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    std::string abstract = makedocabstract(self->query, *pydoc->doc, hlmethods, nohl);
    // Return a python unicode object
    return PyUnicode_Decode(abstract.c_str(), abstract.size(), "UTF-8", "replace");
}

PyDoc_STRVAR(doc_Query_getsnippets,
             "getsnippets(doc, maxoccs = -1, ctxwords = -1, sortbypage=False, "
             "methods = object, nohl=False))\n"
             "Will return a list of snippets for doc by selecting text around the match terms\n"
             "If methods is set, will also perform highlighting. See the highlight method\n"
    );

static PyObject *
Query_getsnippets(recoll_QueryObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Query_getSnippets\n");
    static const char *kwlist[] =
        {"doc", "methods", "maxoccs", "ctxwords", "sortbypage", "nohl", NULL};
    recoll_DocObject *pydoc = 0;
    PyObject *hlmethods = 0;
    int maxoccs = -1;
    int ctxwords = -1;
    PyObject *osortbp = 0;
    bool sortbypage = false;
    PyObject *nohlobj = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|OiiOO:Query_getSnippets", (char **)kwlist,
                                     &recoll_DocType, &pydoc, &hlmethods,  &maxoccs, &ctxwords,
                                     &osortbp, &nohlobj)) {
        return 0;
    }
    if (osortbp && PyObject_IsTrue(osortbp))
        sortbypage = true;
    bool nohl{false};
    if (nohlobj != 0 && PyObject_IsTrue(nohlobj))
        nohl = true;

    if (pydoc->doc == 0) {
        LOGERR("Query_getsnippets: doc not found " << pydoc->doc << "\n");
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }
    if (self->query == 0) {
        LOGERR("Query_getsnippets: query not found " << self->query<< "\n");
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    std::shared_ptr<Rcl::SearchData> sd = self->query->getSD();
    if (!sd) {
        PyErr_SetString(PyExc_ValueError, "Query not initialized");
        return 0;
    }
    std::vector<Rcl::Snippet> snippets;
    PyPlainToRich hler(hlmethods, false, nohl);
    self->query->makeDocAbstract(*(pydoc->doc), &hler, snippets, maxoccs, ctxwords, sortbypage);
    PyObject *sniplist = PyList_New(0);
    for (const auto& snip : snippets) {
        const std::string *textp = &snip.snippet;
        PyList_Append(sniplist,
                      Py_BuildValue(
                          "(iOO)", snip.page,
                          PyUnicode_Decode(snip.term.c_str(), snip.term.size(), "UTF-8", "replace"),
                          PyUnicode_Decode(textp->c_str(), textp->size(), "UTF-8", "replace")));
    }
    return sniplist;
}

PyDoc_STRVAR(doc_Query_getxquery,
             "getxquery(None) -> Unicode string\n"
             "\n"
             "Retrieves the Xapian query description as a Unicode string.\n"
             "Meaningful only after executexx\n"
    );
static PyObject *
Query_getxquery(recoll_QueryObject* self, PyObject *, PyObject *)
{
    LOGDEB0("Query_getxquery self->query " << self->query << "\n");

    if (self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    std::shared_ptr<Rcl::SearchData> sd = self->query->getSD();
    if (!sd) {
        PyErr_SetString(PyExc_ValueError, "Query not initialized");
        return 0;
    }
    string desc = sd->getDescription();
    return PyUnicode_Decode(desc.c_str(), desc.size(), "UTF-8", "replace");
}

PyDoc_STRVAR(doc_Query_getgroups,
             "getgroups(None) -> a list of pairs\n"
             "\n"
             "Retrieves the expanded query terms. Meaningful only after executexx\n"
             "In each pair, the first entry is a list of user terms, the second a list of\n"
             "query terms as derived from the user terms and used in the Xapian Query.\n"
             "The size of each list is one for simple terms, or more for group and phrase\n"
             "clauses\n"
    );
static PyObject *
Query_getgroups(recoll_QueryObject* self, PyObject *, PyObject *)
{
    LOGDEB0("Query_getgroups\n");

    if (self->query == 0) {
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    std::shared_ptr<Rcl::SearchData> sd = self->query->getSD();
    if (!sd) {
        PyErr_SetString(PyExc_ValueError, "Query not initialized");
        return 0;
    }
    HighlightData hld;
    sd->getTerms(hld);
    PyObject *mainlist = PyList_New(0);
    PyObject *ulist;
    PyObject *xlist;
    // We walk the groups vector. For each we retrieve the user group,
    // make a python list of each, then group those in a pair, and
    // append this to the main list.
    for (unsigned int i = 0; i < hld.index_term_groups.size(); i++) {
        HighlightData::TermGroup& tg(hld.index_term_groups[i]);
        unsigned int ugidx = tg.grpsugidx;
        ulist = PyList_New(hld.ugroups[ugidx].size());
        for (unsigned int j = 0; j < hld.ugroups[ugidx].size(); j++) {
            PyList_SetItem(ulist, j, 
                           PyUnicode_Decode(hld.ugroups[ugidx][j].c_str(), 
                                            hld.ugroups[ugidx][j].size(), 
                                            "UTF-8", "replace"));
        }

        // Not sure that this makes any sense after we changed from
        // multiply_groups to using or-plists. TBD: check
        if (tg.kind == HighlightData::TermGroup::TGK_TERM) {
            xlist = PyList_New(1);
            PyList_SetItem(xlist, 0,
                           PyUnicode_Decode(tg.term.c_str(), tg.term.size(), "UTF-8", "replace"));
        } else {
            xlist = PyList_New(tg.orgroups.size());
            for (unsigned int j = 0; j < tg.orgroups.size(); j++) {
                PyList_SetItem(xlist, j,
                               PyUnicode_Decode(tg.orgroups[j][0].c_str(),
                                                tg.orgroups[j][0].size(), "UTF-8", "replace"));
            }
        }
        PyList_Append(mainlist,  Py_BuildValue("(OO)", ulist, xlist));
    }
    return mainlist;
}

static PyMethodDef Query_methods[] = {
    {"execute", (PyCFunction)Query_execute, METH_VARARGS|METH_KEYWORDS, 
     doc_Query_execute},
    {"executesd", (PyCFunction)Query_executesd, METH_VARARGS|METH_KEYWORDS, 
     doc_Query_executesd},
    {"next", (PyCFunction)Query_fetchone, METH_NOARGS,
     doc_Query_fetchone},
    {"fetchone", (PyCFunction)Query_fetchone, METH_NOARGS,
     doc_Query_fetchone},
    {"fetchmany", (PyCFunction)Query_fetchmany, METH_VARARGS|METH_KEYWORDS, 
     doc_Query_fetchmany},
    {"close", (PyCFunction)Query_close, METH_NOARGS,
     doc_Query_close},
    {"sortby", (PyCFunction)Query_sortby, METH_VARARGS|METH_KEYWORDS,
     doc_Query_sortby},
    {"highlight", (PyCFunction)Query_highlight, METH_VARARGS|METH_KEYWORDS,
     doc_Query_highlight},
    {"getxquery", (PyCFunction)Query_getxquery, METH_NOARGS,
     doc_Query_getxquery},
    {"getgroups", (PyCFunction)Query_getgroups, METH_NOARGS,
     doc_Query_getgroups},
    {"makedocabstract", (PyCFunction)Query_makedocabstract, 
     METH_VARARGS|METH_KEYWORDS, doc_Query_makedocabstract},
    {"getsnippets", (PyCFunction)Query_getsnippets, 
     METH_VARARGS|METH_KEYWORDS, doc_Query_getsnippets},
    {"scroll", (PyCFunction)Query_scroll, 
     METH_VARARGS|METH_KEYWORDS, doc_Query_scroll},
    {NULL}  /* Sentinel */
};

static PyMemberDef Query_members[] = {
    {(char*)"rownumber", T_INT, offsetof(recoll_QueryObject, next), 0,
     (char*)"Next index to be fetched from results. Normally increments after\n"
     "each fetchone() call, but can be set/reset before the call effect\n"
     "seeking. Starts at 0"
    },
    {(char*)"rowcount", T_INT, offsetof(recoll_QueryObject, rowcount), 
     READONLY, (char*)"Number of records returned by the last execute"
    },
    {(char*)"arraysize", T_INT, offsetof(recoll_QueryObject, arraysize), 0,
     (char*)"Default number of records processed by fetchmany (r/w)"
    },
    {(char*)"connection", T_OBJECT_EX, offsetof(recoll_QueryObject, connection),
     0, (char*)"Connection object this is from"
    },
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(doc_QueryObject,
             "Recoll Query objects are used to execute index searches. \n"
             "They must be created by the Db.query() method.\n"
    );

PyTypeObject recoll_QueryType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_recoll.Query",
    .tp_basicsize = sizeof(recoll_QueryObject),
    .tp_dealloc = (destructor)Query_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_ITER,
    .tp_doc = doc_QueryObject,
    .tp_iter = Query_iter,
    .tp_iternext = Query_iternext,
    .tp_methods = Query_methods,
    .tp_members = Query_members,
    .tp_init = (initproc)Query_init,
    .tp_new = PyType_GenericNew,
};


///////////////////////////////////////////////
////// DB Db object code

static PyObject *
Db_close(recoll_DbObject *self)
{
    LOGDEB("Db_close. self " << self << "\n");
    self->rcldb.reset();
    Py_RETURN_NONE;
}

static void 
Db_dealloc(recoll_DbObject *self)
{
    LOGDEB("Db_dealloc\n");
    PyObject *ret = Db_close(self);
    Py_DECREF(ret);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int
Db_init(recoll_DbObject *self, PyObject *args, PyObject *kwargs)
{
    static const char *kwlist[] = {"confdir", "extra_dbs", "writable", NULL};
    PyObject *extradbs = 0;
    char *confdir = 0;
    int writable = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sOi", (char**)kwlist,
                                     &confdir, &extradbs, &writable))
        return -1;

    // If the user creates several dbs, changing the confdir, we call
    // recollinit repeatedly, which *should* be ok, except that it
    // resets the log file.
    string reason;
    RclConfig *rclconfig;
    if (confdir) {
        string cfd = confdir;
        rclconfig = recollinit(RCLINIT_PYTHON,0,0, reason, &cfd);
    } else {
        rclconfig = recollinit(RCLINIT_PYTHON, 0, 0, reason, 0);
    }
    LOGDEB("Db_init\n");

    if (!rclconfig) {
        PyErr_SetString(PyExc_EnvironmentError, reason.c_str());
        return -1;
    }
    if (!rclconfig->ok()) {
        PyErr_SetString(PyExc_EnvironmentError, "Bad config ?");
        return -1;
    }
    if (writable) {
        // Make sure that we have an updater as there may be non-initialisation calls in other parts
        // of the code
        statusUpdater(rclconfig, true);
    }
    self->rcldb = std::make_shared<Rcl::Db>(rclconfig);
    delete rclconfig;
    if (!self->rcldb->open(writable ? Rcl::Db::DbUpd : Rcl::Db::DbRO)) {
        LOGERR("Db_init: db open error\n");
        PyErr_SetString(PyExc_EnvironmentError, "Can't open index");
        return -1;
    }

    if (extradbs && extradbs != Py_None) {
        if (!PySequence_Check(extradbs)) {
            PyErr_SetString(PyExc_TypeError, "extra_dbs must be a sequence");
            self->rcldb.reset();
            return -1;
        }
        int dbcnt = PySequence_Size(extradbs);
        if (dbcnt == -1) {
            PyErr_SetString(PyExc_TypeError, "extra_dbs could not be sized");
            self->rcldb.reset();
            return -1;
        }
        for (int i = 0; i < dbcnt; i++) {
            PyObject *item = PySequence_GetItem(extradbs, i);
            string dbname;
            if (PyUnicode_Check(item)) {
                PyObject *utf8o = PyUnicode_AsUTF8String(item);
                if (nullptr != utf8o) {
                    dbname = PyBytes_AsString(utf8o);
                    Py_DECREF(utf8o);
                }
            }  else if (PyBytes_Check(item)) {
                dbname = PyBytes_AsString(item);
            }
            if (dbname.empty()) {
                PyErr_SetString(PyExc_TypeError, "extra_dbs items must be bytes or strings");
                self->rcldb.reset();
                Py_DECREF(item);
                return -1;
            }
            Py_DECREF(item);
            string errmsg = string("extra db could not be opened: ") + dbname;
            if (!self->rcldb->addQueryDb(dbname)) {
                PyErr_SetString(PyExc_EnvironmentError, errmsg.c_str());
                self->rcldb.reset();
                return -1;
            }
        }
    }

    return 0;
}

static PyObject *
Db_query(recoll_DbObject* self)
{
    LOGDEB("Db_query\n");
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    recoll_QueryObject *result = (recoll_QueryObject *)
        PyObject_CallObject((PyObject *)&recoll_QueryType, 0);
    if (!result)
        return 0;
    result->query = new Rcl::Query(self->rcldb.get());
    result->connection = self;
    Py_INCREF(self);

    return (PyObject *)result;
}

static PyObject *
Db_doc(recoll_DbObject* self)
{
    LOGDEB("Db_doc\n");
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    recoll_DocObject *result =
        (recoll_DocObject *)PyObject_CallObject((PyObject *)&recoll_DocType, 0);
    if (!result)
        return 0;
    result->rcldb = self->rcldb;

    return (PyObject *)result;
}

static PyObject *
Db_getDoc(recoll_DbObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("Db_getDoc\n");
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    int idxidx = 0;
    PyObject *pyudi = 0;
    static const char *kwlist[] = {"udi", "idxidx", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i", (char**)kwlist, &pyudi, &idxidx)) {
        return 0;
    }
    std::string udi = stringfromobject(pyudi);

    recoll_DocObject *pydoc =
        (recoll_DocObject *)PyObject_CallObject((PyObject *)&recoll_DocType, 0);
    if (!pydoc)
        return 0;
    if (!self->rcldb->getDoc(std::string(udi), idxidx, *(pydoc->doc), true)) {
        PyErr_SetString(PyExc_AttributeError, "Doc not found: bad UDI or idx index");
        return 0;
    }
    pydoc->rcldb = self->rcldb;
    return (PyObject *)pydoc;
}

static PyObject *
Db_setAbstractParams(recoll_DbObject *self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Db_setAbstractParams\n");
    static const char *kwlist[] = {"maxchars", "contextwords", NULL};
    int ctxwords = -1, maxchars = -1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ii", (char**)kwlist,
                                     &maxchars, &ctxwords))
        return 0;
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    LOGDEB0("Db_setAbstractParams: mxchrs " << maxchars << ", ctxwrds " << ctxwords << "\n");
    self->rcldb->setAbstractParams(-1, maxchars, ctxwords);
    Py_RETURN_NONE;
}

static PyObject *
Db_makeDocAbstract(recoll_DbObject* self, PyObject *args)
{
    LOGDEB0("Db_makeDocAbstract\n");
    recoll_DocObject *pydoc = 0;
    recoll_QueryObject *pyquery = 0;
    if (!PyArg_ParseTuple(args, "O!O!:Db_makeDocAbstract",
                          &recoll_DocType, &pydoc, &recoll_QueryType, &pyquery)) {
        return 0;
    }
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    if (pydoc->doc == 0) {
        LOGERR("Db_makeDocAbstract: doc not found " << pydoc->doc << "\n");
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }
    if (pyquery->query == 0) {
        LOGERR("Db_makeDocAbstract: query not found "<< pyquery->query << "\n");
        PyErr_SetString(PyExc_AttributeError, "query");
        return 0;
    }
    string abstract = makedocabstract(pyquery->query, *pydoc->doc, nullptr);
    // Return a python unicode object
    return PyUnicode_Decode(abstract.c_str(), abstract.size(), "UTF-8", "replace");
}

PyDoc_STRVAR(
    doc_Db_termMatch,
    "termMatch(match_type='wildcard|regexp|stem', expr, field='', "
    "maxlen=-1, casesens=False, diacsens=False, lang='english', freqs=False)"
    " returns the expanded term list\n"
    "\n"
    "Expands the input expression according to the mode and parameters and "
    "returns the expanded term list, as raw terms if freqs is False, or "
    "(term, totcnt, docnt) tuples if freqs is True.\n"
    );
static PyObject *
Db_termMatch(recoll_DbObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Db_termMatch\n");
    static const char *kwlist[] = {"type", "expr", "field", "maxlen", "casesens",
                                   "diacsens", "freqs", "lang", NULL};
    char *tp = 0;
    char *expr = 0; // needs freeing
    char *field = 0; // needs freeing
    int maxlen = -1;
    PyObject *casesens = 0;
    PyObject *diacsens = 0;
    PyObject *freqs = 0;
    char *lang = 0; // needs freeing
    PyObject *ret = 0;
    int typ_sens = 0;
    Rcl::TermMatchResult result(true); /* strip prefixes in results */
    bool showfreqs = false;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ses|esiOOOes", 
                                     (char**)kwlist,
                                     &tp, "utf-8", &expr, "utf-8", &field, 
                                     &maxlen,
                                     &casesens, &diacsens, &freqs,
                                     "utf-8", &lang))
        return 0;

    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        goto out;
    }

    if (!strcasecmp(tp, "wildcard")) {
        typ_sens = Rcl::Db::ET_WILD;
    } else if (!strcasecmp(tp, "regexp")) {
        typ_sens = Rcl::Db::ET_REGEXP;
    } else if (!strcasecmp(tp, "stem")) {
        typ_sens = Rcl::Db::ET_STEM;
    } else {
        PyErr_SetString(PyExc_AttributeError, "Bad type arg");
        goto out;
    }
    
    if (casesens != 0 && PyObject_IsTrue(casesens)) {
        typ_sens |= Rcl::Db::ET_CASESENS;
    }
    if (diacsens != 0 && PyObject_IsTrue(diacsens)) {
        typ_sens |= Rcl::Db::ET_DIACSENS;
    }
    if (freqs != 0 && PyObject_IsTrue(freqs)) {
        showfreqs = true;
    }
    if (!self->rcldb->termMatch(typ_sens, lang ? lang : "english", 
                             expr, result, maxlen, field ? field : "")) {
        LOGERR("Db_termMatch: db termMatch error\n");
        PyErr_SetString(PyExc_AttributeError, "rcldb termMatch error");
        goto out;
    }

    ret = PyList_New(result.entries.size());
    for (unsigned int i = 0; i < result.entries.size(); i++) {
        PyObject *term = PyUnicode_FromString(result.entries[i].term.c_str());
        if (showfreqs) {
            PyObject *totcnt = PyLong_FromLong(result.entries[i].wcf);
            PyObject *doccnt = PyLong_FromLong(result.entries[i].docs);
            PyObject *tup = PyTuple_New(3);
            PyTuple_SetItem(tup, 0, term);
            PyTuple_SetItem(tup, 1, totcnt);
            PyTuple_SetItem(tup, 2, doccnt);
            PyList_SetItem(ret, i, tup);
        } else {
            PyList_SetItem(ret, i, term);
        }
    }

out:
    PyMem_Free(expr);
    PyMem_Free(field);
    PyMem_Free(lang);
    return ret;
}

static PyObject *
Db_setSynonymsFile(recoll_DbObject *self, PyObject *args)
{
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    PyBytesObject *py_path = NULL;
    char *c_path;
    Py_ssize_t path_size;
    if (!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &py_path) || py_path == NULL)
        return 0;
    PyBytes_AsStringAndSize((PyObject *)py_path, &c_path, &path_size);
    if (!self->rcldb->setSynGroupsFile(c_path)) {
        LOGERR("Db_setSynonymsFile: setSynGroupsFile failed\n");
        PyErr_SetString(PyExc_AttributeError, "setSynGroupsFile failed");
        return 0;
    }
    Py_RETURN_NONE;
}

static PyObject *
Db_needUpdate(recoll_DbObject* self, PyObject *args, PyObject *kwds)
{
    LOGDEB0("Db_needUpdate\n");
    char *udi = 0; // needs freeing
    char *sig = 0; // needs freeing
    if (!PyArg_ParseTuple(args, "eses:Db_needUpdate", "utf-8", &udi, "utf-8", &sig)) {
        return 0;
    }
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        PyMem_Free(udi);
        PyMem_Free(sig);
        return 0;
    }
    bool result = self->rcldb->needUpdate(udi, sig);
    PyMem_Free(udi);
    PyMem_Free(sig);
    return Py_BuildValue("i", result);
}

static PyObject *
Db_delete(recoll_DbObject* self, PyObject *args, PyObject *kwds)
{
    LOGDEB0("Db_delete\n");
    char *udi = 0; // needs freeing
    if (!PyArg_ParseTuple(args, "es:Db_delete", "utf-8", &udi)) {
        return 0;
    }
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        PyMem_Free(udi);
        return 0;
    }
    bool result = self->rcldb->purgeFile(udi);
    PyMem_Free(udi);
    return Py_BuildValue("i", result);
}

static PyObject *
Db_preparePurge(recoll_DbObject* self, PyObject *args, PyObject *kwds)
{
    LOGDEB0("Db_preparePurge\n");
    char *backend = 0; // needs freeing
    if (!PyArg_ParseTuple(args, "es:Db_preparePurge", "utf-8", &backend)) {
        return 0;
    }
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        PyMem_Free(backend);
        return 0;
    }
    bool result = self->rcldb->preparePurge(backend);
    return Py_BuildValue("i", result);
}

static PyObject *
Db_purge(recoll_DbObject* self)
{
    LOGDEB0("Db_purge\n");
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    bool result = self->rcldb->purge();
    return Py_BuildValue("i", result);
}

#if PY_MAJOR_VERSION >=3
static PyObject*
Db_createStemDbs(recoll_DbObject* self, PyObject* args) {
    PyObject* pylangs;
    std::vector<std::string> langs;
    bool opret;
    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    if (!PyArg_ParseTuple(args, "O", &pylangs)) {
        return NULL;
    }
    if (PyUnicode_Check(pylangs)) {
        Py_ssize_t sz;
        const char *bytes = PyUnicode_AsUTF8AndSize(pylangs, &sz);
        langs.push_back(std::string(bytes, sz));
    } else {
        if (!PySequence_Check(pylangs)) {
            PyErr_SetString(PyExc_TypeError, "Input must be a list or tuple.");
            return NULL;
        }
        PyObject  *seq = PySequence_Fast(pylangs, "createStemDbs: input must be str or sequence");
        if (NULL == seq) {
            return NULL;
        }
        Py_ssize_t lsz = PySequence_Fast_GET_SIZE(seq);
        for (int i = 0; i < lsz; i++) {
            Py_ssize_t sz;
            const char *bytes;
            PyObject *pylang = PySequence_Fast_GET_ITEM(seq, i);
            if (!PyUnicode_Check(pylang)) {
              PyErr_SetString(PyExc_TypeError, "Input must be a list or tuple of str.");
              return NULL;
            }
            bytes = PyUnicode_AsUTF8AndSize(pylang, &sz);
            langs.push_back(std::string(bytes, sz));
        }
    }
    opret = self->rcldb->createStemDbs(langs);
    if (opret) {
        Py_RETURN_NONE;
    } else {
        PyErr_SetString(PyExc_SystemError, "Db update failed");
        return NULL;
    }
}
#endif /* Python3 */

static PyObject *
Db_addOrUpdate(recoll_DbObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB0("Db_addOrUpdate\n");
    static const char *kwlist[] = {"udi", "doc", "parent_udi", "metaonly", NULL};
    PyObject *pyudi = nullptr;
    PyObject *pyparent_udi = nullptr;
    recoll_DocObject *pydoc;
    int metaonly = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO!|Op:Db_addOrUpdate", (char **)kwlist,
                                     &pyudi, &recoll_DocType, &pydoc,  &pyparent_udi, &metaonly)) {
        return 0;
    }
    string udi = stringfromobject(pyudi);
    string parent_udi = pyparent_udi ? stringfromobject(pyparent_udi) : std::string();

    if (!self->rcldb) {
        PyErr_SetString(PyExc_AttributeError, "db");
        return 0;
    }
    if (pydoc->doc == 0) {
        LOGERR("Db_addOrUpdate: doc not found " << pydoc->doc << "\n");
        PyErr_SetString(PyExc_AttributeError, "doc");
        return 0;
    }
    if (metaonly) {
        pydoc->doc->metaonly = 1;
    }
    if (!self->rcldb->addOrUpdate(udi, parent_udi, *pydoc->doc)) {
        LOGERR("Db_addOrUpdate: rcldb error\n");
        PyErr_SetString(PyExc_AttributeError, "rcldb error");
        return 0;
    }
    Py_RETURN_NONE;
}
    
static PyMethodDef Db_methods[] = {
    {"close", (PyCFunction)Db_close, METH_NOARGS,
     "close() closes the index connection. The object is unusable after this."
    },
    {"query", (PyCFunction)Db_query, METH_NOARGS,
     "query() -> Query. Return a new, blank query object for this index."
    },
    {"doc", (PyCFunction)Db_doc, METH_NOARGS,
     "doc() -> Doc. Return a new, blank doc object for this index."
    },
    {"getDoc",(PyCFunction)Db_getDoc, METH_VARARGS|METH_KEYWORDS,
     "getDoc(udi, idxidx=0) -> Doc.\n"
     "Retrieve document from given udi and index number (default 0, main index)."
    },
    {"cursor", (PyCFunction)Db_query, METH_NOARGS,
     "cursor() -> Query. Alias for query(). Return query object."
    },
    {"setAbstractParams", (PyCFunction)Db_setAbstractParams, METH_VARARGS|METH_KEYWORDS,
     "setAbstractParams(maxchars, contextwords).\n"
     "Set the parameters used to build 'keyword-in-context' abstracts"
    },
    {"makeDocAbstract", (PyCFunction)Db_makeDocAbstract, METH_VARARGS,
     "makeDocAbstract(Doc, Query) -> string\n"
     "Build and return 'keyword-in-context' abstract for document\n"
     "and query."
    },
    {"termMatch", (PyCFunction)Db_termMatch, METH_VARARGS|METH_KEYWORDS, doc_Db_termMatch},
    {"setSynonymsFile", (PyCFunction)Db_setSynonymsFile, METH_VARARGS,
     "setSynonymsFile(path)\n"
     " Set the synonyms file used when querying."
    },
    {"needUpdate", (PyCFunction)Db_needUpdate, METH_VARARGS,
     "needUpdate(udi, sig) -> Bool.\n"
     "Check if the index is up to date for the document defined by udi,\n"
     "having the current signature sig."
    },
    {"delete", (PyCFunction)Db_delete, METH_VARARGS,
     "delete(udi) -> Bool.\n"
     "Purge index from all data for udi. If udi matches a container\n"
     "document, purge all subdocs (docs with a parent_udi matching udi)."
    },
    {"preparePurge", (PyCFunction)Db_preparePurge, METH_VARARGS,
     "preparePurge(backend_name) -> Bool.\n"
     "Mark all documents which do *not* belong to this indexer as present.\n"
     "Mandatory call before starting an update if there are other backends for\n"
     "this index and you are going to call purge() after the update, else all\n"
     "documents for other backends will be deleted from the index by the purge.\n"
    },
    {"purge", (PyCFunction)Db_purge, METH_NOARGS,
     "purge() -> Bool.\n"
     "Delete all documents that were not touched during the just finished\n"
     "indexing pass (since open-for-write). These are the documents for\n"
     "the needUpdate() call was not performed, indicating that they no\n"
     "longer exist in the primary storage system.\n"
    },
    {"addOrUpdate", (PyCFunction)Db_addOrUpdate, METH_VARARGS|METH_KEYWORDS,
     "addOrUpdate(udi, doc, parent_udi=None) -> None\n"
     "Add or update index data for a given document\n"
     "The udi string must define a unique id for the document. It is not\n"
     "interpreted inside Recoll\n"
     "doc is a Doc object\n"
     "if parent_udi is set, this is a unique identifier for the\n"
     "top-level container (ie mbox file)"
    },
#if PY_MAJOR_VERSION >=3
    {"createStemDbs", (PyCFunction)Db_createStemDbs, METH_VARARGS,
     "createStemDbs(lang|lang sequence) -> None\n"
     "Create stemming dictionaries for the specified languages\n"
    },
#endif
    {NULL}  /* Sentinel */
};
PyDoc_STRVAR(doc_DbObject,
             "Db([confdir=None], [extra_dbs=None], [writable = False])\n"
             "\n"
             "A Db object holds a connection to a Recoll index. Use the connect()\n"
             "function to create one.\n"
             "confdir specifies a Recoll configuration directory (default: \n"
             " $RECOLL_CONFDIR or ~/.recoll).\n"
             "extra_dbs is a list of external databases (xapian directories)\n"
             "writable decides if we can index new data through this connection\n"
    );

static PyTypeObject recoll_DbType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_recoll.Db",
    .tp_basicsize = sizeof(recoll_DbObject),
    .tp_dealloc = (destructor)Db_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = doc_DbObject,
    .tp_methods = Db_methods,
    .tp_init = (initproc)Db_init,
    .tp_new = PyType_GenericNew,
};


//////////////////////////////////////////////////////////////////////////
// Module methods
static PyObject *
recoll_connect(PyObject *self, PyObject *args, PyObject *kwargs)
{
    LOGDEB2("recoll_connect\n");
    recoll_DbObject *db = (recoll_DbObject *)
        PyObject_Call((PyObject *)&recoll_DbType, args, kwargs);
    return (PyObject *)db;
}

PyDoc_STRVAR(doc_connect,
             "connect([confdir=None], [extra_dbs=None], [writable = False])\n"
             "         -> Db.\n"
             "\n"
             "Connects to a Recoll database and returns a Db object.\n"
             "confdir specifies a Recoll configuration directory\n"
             "(the default is built like for any Recoll program).\n"
             "extra_dbs is a list of external databases (xapian directories)\n"
             "writable decides if we can index new data through this connection\n"
    );

static PyMethodDef recoll_methods[] = {
    {"connect",  (PyCFunction)recoll_connect, METH_VARARGS|METH_KEYWORDS, 
     doc_connect},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyDoc_STRVAR(pyrecoll_doc_string,
             "This is an interface to the Recoll full text indexer.");

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

#if PY_MAJOR_VERSION >= 3
static int recoll_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int recoll_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_recoll",
    NULL,
    sizeof(struct module_state),
    recoll_methods,
    NULL,
    recoll_traverse,
    recoll_clear,
    NULL
};

#define INITERROR return NULL

PyMODINIT_FUNC PyInit__recoll(void)

#else
#define INITERROR return

    PyMODINIT_FUNC
    init_recoll(void)
#endif
{
    // Note: we can't call recollinit here, because the confdir is only really
    // known when the first db object is created (it is an optional parameter).
    // Using a default here may end up with variables such as stripchars being
    // wrong

#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("_recoll", recoll_methods);
#endif
    if (module == NULL)
        INITERROR;

    struct module_state *st = GETSTATE(module);
    // The first parameter is a char *. Hopefully we don't initialize
    // modules too often...
    st->error = PyErr_NewException(strdup("_recoll.Error"), NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }
    
    if (PyType_Ready(&recoll_DbType) < 0)
        INITERROR;
    Py_INCREF((PyObject*)&recoll_DbType);
    PyModule_AddObject(module, "Db", (PyObject *)&recoll_DbType);

    if (PyType_Ready(&recoll_QueryType) < 0)
        INITERROR;
    Py_INCREF((PyObject*)&recoll_QueryType);
    PyModule_AddObject(module, "Query", (PyObject *)&recoll_QueryType);

    if (PyType_Ready(&recoll_DocType) < 0)
        INITERROR;
    Py_INCREF((PyObject*)&recoll_DocType);
    PyModule_AddObject(module, "Doc", (PyObject *)&recoll_DocType);

    if (PyType_Ready(&recoll_SearchDataType) < 0)
        INITERROR;
    Py_INCREF((PyObject*)&recoll_SearchDataType);
    PyModule_AddObject(module, "SearchData", (PyObject *)&recoll_SearchDataType);

    PyModule_AddStringConstant(module, "__doc__", pyrecoll_doc_string);

    if (PyType_Ready(&rclx_ExtractorType) < 0)
        INITERROR;
    Py_INCREF(&rclx_ExtractorType);
    PyModule_AddObject(module, "Extractor", (PyObject *)&rclx_ExtractorType);

    if (PyType_Ready(&recoll_QResultStoreType) < 0)
        INITERROR;
    Py_INCREF(&recoll_QResultStoreType);
    PyModule_AddObject(module, "QResultStore", (PyObject *)&recoll_QResultStoreType);

    if (PyType_Ready(&recoll_QRSDocType) < 0)
        INITERROR;
    Py_INCREF((PyObject*)&recoll_QRSDocType);
    PyModule_AddObject(module, "QRSDoc", (PyObject *)&recoll_QRSDocType);
    
#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}
