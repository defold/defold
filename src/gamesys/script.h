#ifndef __SCRIPT_H__
#define __SCRIPT_H__

namespace Script
{
    typedef void* HScript;

    bool    Initialize();
    bool    Finalize();

    HScript New(const void* memory);
    void    Delete(HScript script);
    bool    Run(HScript script, PyObject* self, PyObject* args);
}

#endif //__SCRIPT_H__
