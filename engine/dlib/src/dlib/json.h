#ifndef DM_JSON
#define DM_JSON

#include <dmsdk/dlib/json.h>

namespace dmJson
{
    /**
     * Construct a JSON String representation from a c-string array.
     *
     * @note The caller is responsible to free the memory allocated for the
     * JSON String representation by calling free(..) on the returned pointer.
     *
     * @param array The array to construct JSON from.
     * @param length The length of array.
     *
     * @return A JSON String representation of array.
     */
    const char* CStringArrayToJsonString(const char** array, unsigned int length);
}

#endif // #ifndef DM_JSON
