#include <Python.h>
#include "structmember.h"
#include "gameobject_script.h"
#include "gameobject_common.h"

namespace dmGameObject
{
    PyObject* PythonInstance_new(PyTypeObject *type, PyObject *args, PyObject *kw)
    {
        PythonInstance* self = (PythonInstance *)type->tp_alloc(type, 0);
        self->m_Dict = PyDict_New(); // TODO: Memory leak here? Test with valgrind.
        return (PyObject*) self;
    }

    int PythonInstance_SetAttr(PyObject* o, PyObject* name, PyObject* v)
    {
        PythonInstance* self  = (PythonInstance*) o;

        if (!PyString_Check(name))
        {
            PyErr_SetString(PyExc_TypeError, "attribute name must be string");
            return -1;
        }
        if (strcmp(PyString_AsString(name), "Position") == 0)
        {

            if (!PySequence_Check(v))
            {
                PyErr_SetString(PyExc_TypeError, "value must be a sequence");
                return -1;
            }

            if (PySequence_Length(v) != 3)
            {
                PyErr_SetString(PyExc_TypeError, "value must be a sequence of length 3");
                return -1;
            }

            PyObject* x_obj = PySequence_GetItem(v, 0);
            PyObject* y_obj = PySequence_GetItem(v, 1);
            PyObject* z_obj = PySequence_GetItem(v, 2);

            float x = PyFloat_AsDouble(x_obj);
            float y = PyFloat_AsDouble(y_obj);
            float z = PyFloat_AsDouble(z_obj);

            Py_DECREF(x_obj);
            Py_DECREF(y_obj);
            Py_DECREF(z_obj);

            self->m_Instance->m_Position = Point3(x,y,z);
            return 0;
        }
        else
        {
            return PyObject_GenericSetAttr(o, name, v);
        }
    }

    PyObject* PythonInstance_GetAttr(PyObject* o, PyObject* name)
    {
        PythonInstance* self  = (PythonInstance*) o;
        if (!PyString_Check(name))
        {
            PyErr_SetString(PyExc_TypeError, "attribute name must be string");
            return 0;
        }

        if (strcmp(PyString_AsString(name), "Position") == 0)
        {
            PyObject* pos = PyTuple_New(3);
            PyTuple_SetItem(pos, 0, PyFloat_FromDouble(self->m_Instance->m_Position.getX()));
            PyTuple_SetItem(pos, 1, PyFloat_FromDouble(self->m_Instance->m_Position.getY()));
            PyTuple_SetItem(pos, 2, PyFloat_FromDouble(self->m_Instance->m_Position.getZ()));
            return pos;
        }
        else
        {
            return PyObject_GenericGetAttr(o, name);
        }
    }

    PyTypeObject PythonInstanceType = {
        PyObject_HEAD_INIT(NULL)
        0,                              /*ob_size*/
        "gameobject.Instance",          /*tp_name*/
        sizeof(PythonInstance),         /*tp_basicsize*/
        0,                              /*tp_itemsize*/
        0,                              /*tp_dealloc*/
        0,                              /*tp_print*/
        0,                              /*tp_getattr*/
        0,                              /*tp_setattr*/
        0,                              /*tp_compare*/
        0,                              /*tp_repr*/
        0,                              /*tp_as_number*/
        0,                              /*tp_as_sequence*/
        0,                              /*tp_as_mapping*/
        0,                              /*tp_hash */
        0,                              /*tp_call*/
        0,                              /*tp_str*/
        &PythonInstance_GetAttr,        /*tp_getattro*/
        &PythonInstance_SetAttr,        /*tp_setattro*/
        0,                              /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,             /*tp_flags*/
        "GameObject instance",          /* tp_doc */
        0,                              /* tp_traverse */
        0,                              /* tp_clear */
        0,                              /* tp_richcompare */
        0,                              /* tp_weaklistoffset */
        0,                              /* tp_iter */
        0,                              /* tp_iternext */
        0,                              /* tp_methods */
        0,                              /* tp_members */
        0,                              /* tp_getset */
        0,                              /* tp_base */
        0,                              /* tp_dict */
        0,                              /* tp_descr_get */
        0,                              /* tp_descr_set */
        offsetof(PythonInstance, m_Dict),  /* tp_dictoffset */
        0,                              /* tp_init */
        0,                              /* tp_alloc */
        PythonInstance_new,             /* tp_new */
        0,                              /* tp_free */
    };

    static PyMethodDef python_instance_methods[] = {
        {NULL}  /* Sentinel */
    };

    // TODO: Currently not used.
    char g_LastErrorString[1024] = { '\0' };

    static bool ErrorOccured()
    {
        PyObject* err = PyErr_Occurred();
        if (err)
        {
            PyErr_Print();
            fflush(stderr);
            fflush(stdout);

            PyObject* err_str_obj = PyObject_Str(err);
            char* err_str = PyString_AsString(err_str_obj);
            strncpy(g_LastErrorString, err_str, sizeof(g_LastErrorString));
            g_LastErrorString[sizeof(g_LastErrorString)-1] = '\0';
            Py_DECREF(err_str_obj);

            return true;
        }

        return false;
    }

    void InitializeScript()
    {
        Py_Initialize();

        PyObject* m;

        //PythonInstanceType.tp_new = PyType_GenericNew;
        if (PyType_Ready(&PythonInstanceType) < 0)
            return;

        m = Py_InitModule3("gameobject", python_instance_methods,
                           "GameObject module.");

        Py_INCREF(&PythonInstanceType);
        PyModule_AddObject(m, "Instance", (PyObject *)&PythonInstanceType);
    }

    void FinalizeScript()
    {
        Py_Finalize();
    }

    HScript NewScript(const void* memory)
    {
        if (memory == NULL) return NULL;

        // TODO: Budget. Memory allocation. Due to windows line endings...
        char* normalized_buffer = (char*) malloc(strlen((char*) memory) + 1);
        char*in = (char*) memory;
        char*out = normalized_buffer;
        // Convert to unix line endings
        while (*in)
        {
            char c = *in;
            if (c != '\r')
            {
                *out = *in;
                out++;
            }
            in++;
        }
        *out = '\0';

        PyObject* obj = Py_CompileString((const char*)normalized_buffer, "<script>", Py_file_input);
        free(normalized_buffer);

        if (ErrorOccured() ) return NULL;

        PyObject* glob = PyDict_New();
        PyDict_SetItemString(glob, "__builtins__", PyEval_GetBuiltins());

        PyObject* module = PyEval_EvalCode((PyCodeObject*)obj, glob, glob);
        if (ErrorOccured() ) return NULL;


        Py_DECREF(obj);
        Py_DECREF(module);

        return (HScript)glob;
    }

    void DeleteScript(HScript script)
    {
        Py_DECREF((PyObject*) script);
    }

    bool RunScript(HScript script, const char* function_name, PyObject* self, PyObject* args)
    {
        PyObject* glob = (PyObject*)script;

        PyObject* func = PyDict_GetItemString (glob, function_name);
        if (ErrorOccured() ) return NULL;

        int ret = PyCallable_Check(func);
        if (ret == 0)
        {
            printf("Error in PyCallable_Check()\n");
            return false;
        }

        if (ErrorOccured() ) return false;

        //PyObject* pValue = PyObject_CallObject(func, args);
        if (ErrorOccured() ) return false;

        return true;
    }
}
