#include <Python.h>
#include "script.h"

namespace Script
{
    bool NewEngine()
    {
        Py_Initialize();
        return true;
    }

    bool DeleteEngine()
    {
        Py_Finalize();
        return true;
    }

    HScript New(const void* memory)
    {
        PyObject* obj = Py_CompileString((const char*)memory, "<script>", Py_file_input);

        PyObject* module = PyImport_ExecCodeModule("Update", obj);
        Py_DECREF(obj);

        return (HScript)module;
    }

    void Delete(HScript script)
    {
        Py_DECREF((PyObject*) script);
    }

    bool Run(HScript script, PyObject* self, PyObject* args)
    {
        PyObject* module = (PyObject*)script;

        PyObject* func = PyObject_GetAttrString(module, "Update");
        /* pFunc is a new reference */

        if (func && PyCallable_Check(func))
        {
            PyObject* pValue = PyObject_CallObject(func, args);
        }

        return true;
    }
}
