#define PY_SSIZE_T_CLEAN

#include "Python.h"
#include "bloom.h"

// heap type
// PyModuleDef_Init
// nogil
// module state
typedef struct PyBloomObject
{
    PyObject_HEAD
    struct bloom *ptr;
} PyBloomObject;

static PyObject *
PyBloom_tp_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) // __cinit__  in cython is called here
{
    unsigned int entries = 1000;
    double error = 0.01;
    char *kwlist[] = {"entries", "error", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Id:Bloom.__new__", kwlist, &entries, &error))
    {
        return NULL;
    }

    struct bloom *ptr = (struct bloom *) PyMem_RawMalloc(sizeof(struct bloom));
    if (ptr == NULL)
    {
        return PyErr_NoMemory();
    }
    if(bloom_init2(ptr, entries, error)==1)
    {
        bloom_free(ptr);
        PyMem_RawFree(ptr);
        return PyErr_NoMemory();
    }
    PyBloomObject *self = (PyBloomObject *) subtype->tp_alloc(subtype, 0);
    if (self == NULL)
    {
        bloom_free(ptr);
        PyMem_RawFree(ptr);
        return NULL; // PyType_GenericAlloc will set error here
    }

    self->ptr = ptr;
    return (PyObject *) self;
}

void
PyBloom_tp_dealloc(PyBloomObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    bloom_free(self->ptr);
    PyMem_RawFree(self->ptr);
    self->ptr = NULL;
    tp->tp_free((PyObject *) self);
    Py_DECREF(tp);
#ifdef PYDBG
    fprintf(stderr, "type's refcnt %lld\n", Py_REFCNT(tp)); // 谁tm偷了朕的引用
#endif
}

static PyObject *
PyBloom_check_impl(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyBloomObject *s = (PyBloomObject *) self;
    Py_buffer buffer;
    char *kwlist[] = {"buffer", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s*", kwlist, &buffer))
    {
        return NULL;
    }
    int ret;
    Py_BEGIN_ALLOW_THREADS
        ret = bloom_check(s->ptr, buffer.buf, (int) buffer.len);
    Py_END_ALLOW_THREADS
    PyBuffer_Release(&buffer);
    return PyBool_FromLong(ret);
}

static PyObject *
PyBloom_add_impl(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyBloomObject *s = (PyBloomObject *) self;
    Py_buffer buffer;
    char *kwlist[] = {"buffer", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s*", kwlist, &buffer))
    {
        return NULL;
    }
    int ret;
    Py_BEGIN_ALLOW_THREADS
        ret = bloom_add(s->ptr, buffer.buf, (int) buffer.len);
    Py_END_ALLOW_THREADS
    PyBuffer_Release(&buffer);
    return PyBool_FromLong(ret);
}


static PyObject *
PyBloom_dump_impl(PyObject *self, PyObject *Py_UNUSED(arg))
{
    PyBloomObject *s = (PyBloomObject *) self;
    Py_BEGIN_ALLOW_THREADS
        bloom_print(s->ptr);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject *
PyBloom_reset_impl(PyObject *self, PyObject *Py_UNUSED(arg))
{
    PyBloomObject *s = (PyBloomObject *) self;
    int ret;
    Py_BEGIN_ALLOW_THREADS
        ret = bloom_reset(s->ptr);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ret);
}

static PyObject *
PyBloom_save_impl(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyBloomObject *s = (PyBloomObject *) self;
    char *kwlist[] = {"filename", NULL};
    PyObject * filename = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&", kwlist, PyUnicode_FSConverter, &filename))
    {
        return NULL;
    }
    int ret;
    Py_BEGIN_ALLOW_THREADS
        ret = bloom_save(s->ptr, PyBytes_AS_STRING(filename));
    Py_END_ALLOW_THREADS
    Py_CLEAR(filename);
    return PyBool_FromLong(ret);
}

static PyObject *
PyBloom_load_impl(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyBloomObject *s = (PyBloomObject *) self;
    char *kwlist[] = {"filename", NULL};
    PyObject * filename = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&", kwlist, PyUnicode_FSConverter, &filename))
    {
        return NULL;
    }
    int ret;
    Py_BEGIN_ALLOW_THREADS
        ret = bloom_load(s->ptr, PyBytes_AS_STRING(filename));
    Py_END_ALLOW_THREADS
    Py_CLEAR(filename);
    return PyLong_FromLong(ret);
}

static PyObject *
PyBloom_merge_impl(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyBloomObject *s = (PyBloomObject *) self;
    char *kwlist[] = {"bloom", NULL};
    PyBloomObject * other = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, Py_TYPE(self), &other))
    {
        return NULL;
    }
    int ret;
    Py_BEGIN_ALLOW_THREADS
        ret = bloom_merge(s->ptr, other->ptr);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ret);
}

static PyMethodDef PyBloom_tp_methods[] = {
        {
                "check", (PyCFunction) PyBloom_check_impl, METH_VARARGS | METH_KEYWORDS, PyDoc_STR(
                                                                                                 "Return False if element is not present, True if element is present (or false positive due to collision)")
        },
        {
                "add",   (PyCFunction) PyBloom_add_impl,   METH_VARARGS | METH_KEYWORDS, PyDoc_STR(
                                                                                                 "Return False if element was not present and was added, True if element (or a collision) had already been added previously")
        },
        {       "dump",  (PyCFunction) PyBloom_dump_impl,  METH_NOARGS, NULL},
        {
                "reset", (PyCFunction) PyBloom_reset_impl, METH_NOARGS, PyDoc_STR(
                                                                                "Return False - on success, True - on failure")
        },
        {
                "save", (PyCFunction) PyBloom_save_impl, METH_VARARGS | METH_KEYWORDS, PyDoc_STR(
                                                                                "Return False - on success, True - on failure")
        },
        {
                "load", (PyCFunction) PyBloom_load_impl, METH_VARARGS | METH_KEYWORDS, PyDoc_STR(
                                                                                               "Return 0 on success, > 0 on failure")
        },
        {
                "merge", (PyCFunction) PyBloom_merge_impl, METH_VARARGS | METH_KEYWORDS, PyDoc_STR(
                                                                                               "Return False - on success, True - incompatible bloom filters")
        },
        {NULL, NULL, 0,                                                 NULL}
};


static PyObject *
PyBloom_getentries(PyObject *self, void *ud)
{
    PyBloomObject *s = (PyBloomObject *) self;
    return PyLong_FromUnsignedLong(s->ptr->entries);
}

static PyObject *
PyBloom_getbits(PyObject *self, void *ud)
{
    PyBloomObject *s = (PyBloomObject *) self;
    return PyLong_FromUnsignedLongLong(s->ptr->bits);
}

static PyObject *
PyBloom_getbytes(PyObject *self, void *ud)
{
    PyBloomObject *s = (PyBloomObject *) self;
    return PyLong_FromUnsignedLongLong(s->ptr->bytes);
}

static PyObject *
PyBloom_gethashes(PyObject *self, void *ud)
{
    PyBloomObject *s = (PyBloomObject *) self;
    return PyLong_FromUnsignedLong(s->ptr->hashes);
}

static PyObject *
PyBloom_geterror(PyObject *self, void *ud)
{
    PyBloomObject *s = (PyBloomObject *) self;
    return PyFloat_FromDouble(s->ptr->error);
}


static PyGetSetDef PyBloom_tp_getset[] = {
        {"entries", (getter) PyBloom_getentries, NULL, PyDoc_STR("entries"), NULL},
        {"bits",    (getter) PyBloom_getbits,    NULL, PyDoc_STR("bits"),    NULL},
        {"bytes",   (getter) PyBloom_getbytes,   NULL, PyDoc_STR("bytes"),   NULL},
        {"hashes",  (getter) PyBloom_gethashes,  NULL, PyDoc_STR("hashes"),  NULL},
        {"error",   (getter) PyBloom_geterror,   NULL, PyDoc_STR("error"),   NULL},
        {NULL, NULL,                             NULL, NULL,                 NULL}  /* Sentinel */
};

static PyType_Slot PyBloom_TypeSpec_Slots[] = {
        {Py_tp_new, PyBloom_tp_new},
        {Py_tp_dealloc, PyBloom_tp_dealloc},
        {Py_tp_methods, PyBloom_tp_methods},
        {Py_tp_getset, PyBloom_tp_getset},
        {Py_tp_doc, PyDoc_STR("wraps libbloom's struct bloom")},
        {0, NULL}
};

static PyType_Spec PyBloom_TypeSpec = {
        .name = "_bloom.Bloom",
        .basicsize = sizeof(PyBloomObject),
        .itemsize = 0,
        .flags= Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_MANAGED_WEAKREF,
        .slots = PyBloom_TypeSpec_Slots,
};

static int
bloom_exec(PyObject *mod)
{
    PyObject *tp = PyType_FromSpec(&PyBloom_TypeSpec);
    if (tp == NULL)
    {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "Bloom", tp) < 0)
    {
        Py_DECREF(tp);
        return -1;
    }
    Py_DECREF(tp);
    if(PyModule_AddStringConstant(mod, "version", bloom_version()) < 0)
    {
        return -1;
    }
    return 0;
}


static PyModuleDef_Slot bloom_slots[] = {
#if PY_VERSION_HEX >= 0x030D0000
        {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
        {Py_mod_exec, bloom_exec},
        {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
        {0, NULL}
};


static PyModuleDef bloom_module = {
        PyModuleDef_HEAD_INIT,
        "_bloom",   /* name of module */
        PyDoc_STR("python binding for libbloom"), /* module documentation, may be NULL */
        0,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
        NULL,
        bloom_slots
};

PyMODINIT_FUNC
PyInit__bloom(void)
{
    return PyModuleDef_Init(&bloom_module);
}