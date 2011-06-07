#ifndef DM_VSCRIPT_H
#define DM_VSCRIPT_H

#include <stdint.h>
#include <dlib/hash.h>
#include <ddf/ddf.h>
#include "vscript_ddf.h"

namespace dmVScript
{
    typedef struct Context* HContext;
    typedef struct Script* HScript;
    typedef struct ScriptInstance* HScriptInstance;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_VARIABLE_NOT_FOUND = -1,
    };

    struct NewContextParams
    {

    };

    struct Value
    {
        dmVScriptDDF::Type* m_Type;
        union
        {
            float       m_FloatValue;
            const char* m_StringValue;

            struct
            {
                Value*      m_ListValue;
                uint32_t    m_Length;
            };
        };
    };

    /**
     * Create a new context
     * @param params Context parameters
     * @return New context
     */
    HContext NewContext(const NewContextParams* params);

    /**
     * Delete a context
     * @param context Context to delete
     */
    void DeleteContext(HContext context);

    /**
     * Create a new script
     * @param context Context handle
     * @param buffer Script to load
     * @param buffer_size Script length
     * @param script Script handle (out)
     * @return RESULT_OK on success
     */
    Result NewScript(HContext context, const void* buffer, uint32_t buffer_size, HScript* script);

    /**
     * Delete script
     * @param context Context handle
     * @param script Script to delete
     */
    void DeleteScript(HContext context, HScript script);

    /**
     * Create a new script instance. A script instance represents the state, i.e. global variables, and possibly
     * the state of a coroutine
     * @param script Script handle
     * @param instance Instance handle (out)
     * @return RESULT_OK on success
     */
    Result NewScriptInstance(HScript script, HScriptInstance* instance);

    /**
     * Delete a script instance
     * @param instance Script instance handle
     * @param instance
     */
    void DeleteScriptInstance(HScriptInstance instance);

    /**
     * Invoke a procedure on an instnace
     * @param instance Instance handle
     * @param name_hash Name hash of the procedure
     * @param name Name of the function, used for debugging/logging purposes
     */
    void Invoke(HScriptInstance instance, dmhash_t name_hash, const char* name);

    /**
     * Get global variable in instance
     * @param instance Instance handle
     * @param name Global variable name
     * @param value Value (out)
     * @return RESULT_OK on success, RESULT_VARIABLE_NOT_FOUND if the variable isn't found
     */
    Result GetGlobal(HScriptInstance instance, const char* name, Value* value);

    /**
     * Set global variable in instance
     * @param instance Instance handle
     * @param name Variable name
     * @param value Value to set
     * @return RESULT_OK on success
     */
    Result SetGlobal(HScriptInstance instance, const char* name, const Value* value);
}

#endif
