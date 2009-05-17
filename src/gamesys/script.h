#ifndef __SCRIPT_H__
#define __SCRIPT_H__

typedef void* HScript;


bool    ScriptEngineNew();
bool    ScriptEngineDelete();

HScript ScriptNew(void* memory);
void    ScriptDestroy(HScript script);
bool    ScriptRun(HScript script, PyObject* self, PyObject* args);


#endif //__SCRIPT_H__
