#include <python/Python.h>
#include "script.h"


bool ScriptEngineNew()
{
    Py_Initialize();

    return true;
}

bool ScriptEngineDelete()
{
    Py_Finalize();

    return true;
}
HScript ScriptNew(void* memory)
{

    PyObject* obj = Py_CompileString((const char*)memory, "test.py", Py_file_input);

    PyObject* module = PyImport_ExecCodeModule("Update", obj);
    Py_DECREF(obj);


    return (HScript)module;
}


void ScriptDelete(HScript script)
{

}


bool ScriptRun(HScript script, PyObject* self, PyObject* args)
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
