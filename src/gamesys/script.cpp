#include <Python.h>
#include "script.h"

namespace Script
{
    bool Initialize()
    {
        Py_Initialize();
        return true;
    }

    bool Finalize()
    {
        Py_Finalize();
        return true;
    }

    HScript New(const void* memory)
    {
        assert(memory);

        char* update_func = "Update";

        PyObject* obj = Py_CompileString((const char*)memory, "<script>", Py_file_input);

        PyObject* module = PyImport_ExecCodeModule(update_func, obj);
        PyObject* func = PyObject_GetAttrString(module, update_func);
        Py_DECREF(obj);
        Py_DECREF(module);

        return (HScript)func;
    }

    void Delete(HScript script)
    {
        assert(script);

        Py_DECREF((PyObject*) script);
    }

    bool Run(HScript script, PyObject* self, PyObject* args)
    {
        assert(script);

        PyObject* func = (PyObject*)script;

        if (func && PyCallable_Check(func))
        {
            PyObject* pValue = PyObject_CallObject(func, args);
        }

        return true;
    }
}
