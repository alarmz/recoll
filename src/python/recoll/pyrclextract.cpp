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
#include <bytearrayobject.h>

#include <string>
#include <memory>

#include "log.h"
#include "rcldoc.h"
#include "internfile.h"
#include "rclconfig.h"
#include "rclinit.h"
#include "rcldb.h"

#include "pyrecoll.h"

using namespace std;

//////////////////////////////////////////////////////////////////////
/// Extractor object code
typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    FileInterner *xtr;
    std::shared_ptr<Rcl::Db> rcldb;
    recoll_DocObject *docobject;
    RclConfig *localconfig{nullptr};
} rclx_ExtractorObject;

static void 
Extractor_dealloc(rclx_ExtractorObject *self)
{
    LOGDEB("Extractor_dealloc\n" );
    if (self->docobject) {
        Py_DECREF(&self->docobject);
    }
    delete self->xtr;
    self->rcldb.reset();
    deleteZ(self->localconfig);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int
Extractor_init(rclx_ExtractorObject *self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("Extractor_init\n" );
    static const char* kwlist[] = {"doc", NULL};
    recoll_DocObject *dobj = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:Extractor_init", (char**)kwlist,
                                     &recoll_DocType, &dobj))
        return -1;
    if (dobj->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "Null Doc ?");
        return -1;
    }
    self->docobject = dobj;
    Py_INCREF(dobj);

    self->rcldb = dobj->rcldb;
    if (nullptr == self->localconfig)
        self->localconfig = new RclConfig(*self->rcldb->getConf());
    self->xtr = new FileInterner(*dobj->doc, self->localconfig, FileInterner::FIF_forPreview);
    return 0;
}

PyDoc_STRVAR(doc_Extractor_textextract,
             "textextract(ipath)\n"
             "Extract document defined by ipath and return a doc object. The doc.text\n"
             "field has the document text as either text/plain or text/html\n"
             "according to doc.mimetype.\n"
    );

static PyObject *
Extractor_textextract(rclx_ExtractorObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("Extractor_textextract\n" );
    static const char* kwlist[] = {"ipath", NULL};
    char *sipath = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "es:Extractor_textextract", (char**)kwlist,
                                     "utf-8", &sipath))
        return 0;

    string ipath(sipath);
    PyMem_Free(sipath);

    if (self->xtr == 0) {
        PyErr_SetString(PyExc_AttributeError, "extract: null object");
        return 0;
    }
    /* Call the doc class object to create a new doc. */
    recoll_DocObject *result =
        (recoll_DocObject *)PyObject_CallObject((PyObject *)&recoll_DocType, 0);
    if (!result) {
        PyErr_SetString(PyExc_AttributeError, "extract: doc create failed");
        return 0;
    }
    result->rcldb = self->rcldb;

    FileInterner::Status status = self->xtr->internfile(*(result->doc), ipath);
    if (status != FileInterner::FIDone && status != FileInterner::FIAgain) {
        PyErr_SetString(PyExc_AttributeError, "internfile failure");
        return 0;
    }

    string html = self->xtr->get_html();
    if (!html.empty()) {
        result->doc->text = html;
        result->doc->mimetype = "text/html";
    }

    // Is this actually needed ? Useful for url which is also formatted .
    Rcl::Doc *doc = result->doc;
    printableUrl(self->rcldb->getConf()->getDefCharset(), doc->url, doc->meta[Rcl::Doc::keyurl]);
    doc->meta[Rcl::Doc::keytp] = doc->mimetype;
    doc->meta[Rcl::Doc::keyipt] = doc->ipath;
    doc->meta[Rcl::Doc::keyfs] = doc->fbytes;
    doc->meta[Rcl::Doc::keyds] = doc->dbytes;
    return (PyObject *)result;
}

PyDoc_STRVAR(doc_Extractor_idoctofile,
             "idoctofile(ipath='', mimetype='', ofilename='')\n"
             "Extract document defined by ipath into a file, in its native format.\n"
    );
static PyObject *
Extractor_idoctofile(rclx_ExtractorObject* self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("Extractor_idoctofile\n" );
    static const char* kwlist[] = {"ipath", "mimetype", "ofilename", NULL};
    char *sipath = 0;
    char *smt = 0;
    char *soutfile = 0; // no freeing
    if (!PyArg_ParseTupleAndKeywords(args,kwargs, "eses|s:Extractor_idoctofile", (char**)kwlist,
                                     "utf-8", &sipath, "utf-8", &smt, &soutfile))
        return 0;

    string ipath(sipath);
    PyMem_Free(sipath);
    string mimetype(smt);
    PyMem_Free(smt);
    string outfile;
    if (soutfile && *soutfile)
        outfile.assign(soutfile); 
    
    if (self->xtr == 0) {
        PyErr_SetString(PyExc_AttributeError, "idoctofile: null object");
        return 0;
    }

    if (nullptr == self->localconfig)
        self->localconfig = new RclConfig(*self->rcldb->getConf());
    // If ipath is empty and we want the original mimetype, we can't use
    // FileInterner::internToFile() because the first conversion was performed by the FileInterner
    // constructor, so that we can't reach the original object this way. Instead, if the data comes
    // from a file (m_fn set), we just copy it, else, we call idoctofile, which will call
    // topdoctofile (and re-fetch the data, yes, wastefull)
    TempFile temp;
    bool status = false;
    LOGDEB("Extractor_idoctofile: ipath [" << ipath << "] mimetype [" << mimetype <<
           "] doc mimetype [" << self->docobject->doc->mimetype << "\n");
    if (ipath.empty() && !mimetype.compare(self->docobject->doc->mimetype)) {
        status = FileInterner::idocToFile(temp, outfile, self->localconfig, *self->docobject->doc);
    } else {
        self->xtr->setTargetMType(mimetype);
        status = self->xtr->interntofile(temp, outfile, ipath, mimetype);
    }
    if (!status) {
        PyErr_SetString(PyExc_AttributeError, "interntofile failure");
        return 0;
    }
    if (outfile.empty())
        temp.setnoremove(1);
    PyObject *result = outfile.empty() ? PyBytes_FromString(temp.filename()) :
        PyBytes_FromString(outfile.c_str());
    return (PyObject *)result;
}

static PyMethodDef Extractor_methods[] = {
    {"textextract", (PyCFunction)Extractor_textextract, 
     METH_VARARGS|METH_KEYWORDS, doc_Extractor_textextract},
    {"idoctofile", (PyCFunction)Extractor_idoctofile, 
     METH_VARARGS|METH_KEYWORDS, doc_Extractor_idoctofile},
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(doc_ExtractorObject,
             "Extractor()\n"
             "\n"
             "An Extractor object can extract data from a native simple or compound\n"
             "object.\n"
    );

PyTypeObject rclx_ExtractorType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_rclextract.Extractor",
    .tp_basicsize = sizeof(rclx_ExtractorObject),
    .tp_dealloc = (destructor)Extractor_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = doc_ExtractorObject,
    .tp_methods = Extractor_methods,
    .tp_init = (initproc)Extractor_init,
    .tp_new = PyType_GenericNew,
};
