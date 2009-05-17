#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include <Python.h>


namespace Script
{
    typedef void* HScript;

    bool    Initialize();
    bool    Finalize();

    HScript New(const void* memory);
    void    Delete(HScript script);
    bool    Run(HScript script, const char* function_name, PyObject* self, PyObject* args);
}

#endif //__SCRIPT_H__
