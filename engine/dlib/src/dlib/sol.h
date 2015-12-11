#ifndef DM_SOL
#define DM_SOL

#include <stdint.h>
#include <stddef.h>

#include <sol/runtime.h>
#include <sol/reflect.h>

namespace dmSol
{
    typedef uint32_t Handle;

    /**
     * Initialize sol and the runtime.
     * @param max_handles Maximum number of hanldes
     */
    void Initialize(uint32_t max_handles = 64);

    /**
     * Shut down the runtime and clean up
     */
    void Finalize();

    /**
     * Shut down the runtime and assert on number of live allocations being the same
     * as after static initalization. If this asserts you know objects are being leaked.
     */
    void FinalizeWithCheck();

    /**
     * Create a handle, identified with a type token. The pointer value can only be retreived
     * by calling GetHandle with the same handle and same type ptr. Handles are meant for short-lived
     * objects, for instance stack ones, as creating a handle genereates no heap allocation and is quick.
     *
     * @param ptr Pointer value
     * @param type Type token
     */
    Handle MakeHandle(void* ptr, void* type);

    /**
     * Get value of a handle. Returns the handle pointer value if it is still valid and
     * the type is correct.
     * @param ptr Pointer value
     * @param type Type token
     * @return pointer held by handle
     */
    void* GetHandle(Handle handle, void* type);

    /**
     * Check if handle is still valid
     * @param handle Handle
     * @param type Type token
     * @return if handle is valid
     */
    bool IsHandleValid(Handle handle, void* type);

    /**
     * Release a handle.
     * @param handle handle
     */
    void FreeHandle(Handle handle);

    typedef struct Proxy* HProxy;

    /**
     * Allocate a proxy object through a garbage collected allocation.
     * Useful for when you have no idea in advance how many you need, but costs a
     * mem alloc.
     * @param ptr Pointer to hold
     * return Proxy object
     */
    HProxy NewProxy(void* ptr);

    /**
     * Free a proxy. Note that this does not release the memory immediately until
     * the garbage collector decides to.
     * @param proxy Proxy
     */
    void DeleteProxy(HProxy proxy);

    /**
     * Get value pointed to by the proxy, unless the proxy has been DeletProxyed before,
     * in which case null is returned. Use this when receiving proxy objects from sol code
     * @param proxy Proxy
     * @return pointer value held by the proxy
     */
    void* ProxyGet(HProxy proxy);

    /**
     * Duplicate a string into a sol-compatible string. Release with runtime_unpin
     * @param string to duplicate
     * @return sol-allocated string duplicate
     */
    const char* StrDup(const char* str);

    /**
     * Calls runtime_pin if the any object is a reference to an object, otherwise do nothing
     * @param any value
     */
    void SafePin(::Any);

    /**
     * Calls runtime_unpin if the any object is a reference to an object, otherwise do nothing
     * @param any value
     */
    void SafeUnpin(::Any);

    /**
     * Returns the sizee of the struct referenced by the any value
     * @param ptr the value
     * @return size of the struct, 0 on error
     */
    size_t SizeOf(::Any ptr);
}

#endif
