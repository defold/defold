#include <Python.h>
#include "script.h"

namespace Script
{

    void CheckErrors()
    {
        if ( PyErr_Occurred() )
        {
            PyErr_Print();
            fflush(stderr);
            fflush(stdout);
        }
    }

    bool Initialize()
    {
        Py_Initialize();
        CheckErrors();
        return true;
    }

    bool Finalize()
    {
        Py_Finalize();
        CheckErrors();
        return true;
    }

    HScript New(const void* memory)
    {
        char* update_func = "Update";

        PyObject* obj = Py_CompileString((const char*)memory, "<script>", Py_file_input);
        CheckErrors();

        PyObject* module = PyImport_ExecCodeModule(update_func, obj);
        CheckErrors();

        PyObject* func = PyObject_GetAttrString(module, update_func);
        CheckErrors();

        Py_DECREF(obj);
        Py_DECREF(module);

        return (HScript)func;
    }

    void Delete(HScript script)
    {
        Py_DECREF((PyObject*) script);
    }

    bool Run(HScript script, PyObject* self, PyObject* args)
    {
        PyObject* func = (PyObject*)script;

        if (func && PyCallable_Check(func))
        {
            PyObject* pValue = PyObject_CallObject(func, args);
        }

        return true;
    }
}
