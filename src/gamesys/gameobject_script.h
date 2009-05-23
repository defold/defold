#ifndef __GAMEOBJECTSCRIPT_H__
#define __GAMEOBJECTSCRIPT_H__

#include <Python.h>

/**
 * Private header for GameObject
 */

namespace GameObject
{
    struct Instance;

    typedef struct {
        PyObject_HEAD
        Instance* m_Instance;
        PyObject* m_Dict;
    } PythonInstance;

    typedef void* HScript;

    extern PyTypeObject PythonInstanceType;

    void    InitializeScript();
    void    FinalizeScript();

    HScript NewScript(const void* memory);
    void    DeleteScript(HScript script);
    bool    RunScript(HScript script, const char* function_name, PyObject* self, PyObject* args);
}

#endif //__GAMEOBJECTSCRIPT_H__
