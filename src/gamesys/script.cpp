#include <Python.h>
#include "script.h"

namespace Script
{

    static bool ErrorOccured()
    {
        if ( PyErr_Occurred() )
        {
            PyErr_Print();
            fflush(stderr);
            fflush(stdout);

            return true;
        }

        return false;
    }

    bool Initialize()
    {
        Py_Initialize();

        if (ErrorOccured() ) return false;

        return true;
    }

    bool Finalize()
    {
        Py_Finalize();

        return true;
    }

    HScript New(const void* memory)
    {
        char* update_func = "Update";

        if (memory == NULL) return NULL;

        PyObject* obj = Py_CompileString((const char*)memory, "<script>", Py_file_input);
        if (ErrorOccured() ) return NULL;

        PyObject* module = PyImport_ExecCodeModule(update_func, obj);
        if (ErrorOccured() ) return NULL;

        PyObject* func = PyObject_GetAttrString(module, update_func);
        if (ErrorOccured() ) return NULL;

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

        int ret = PyCallable_Check(func);
        if (ret == 0)
        {
            printf("Error in PyCallable_Check()\n");
            return false;
        }

        if (ErrorOccured() ) return false;

        PyObject* pValue = PyObject_CallObject(func, args);
        if (ErrorOccured() ) return false;

        return true;
    }
}
