#ifndef DM_JSON
#define DM_JSON

/**
 * Json parsing
 */
namespace dmJson
{
    /**
     * Result
     */
    enum Result
    {
        RESULT_OK = 0,           //!< RESULT_OK
        RESULT_SYNTAX_ERROR = -1,//!< RESULT_SYNTAX_ERROR
        RESULT_INCOMPLETE = -2,  //!< RESULT_INCOMPLETE
        RESULT_UNKNOWN = -1000,  //!< RESULT_UNKNOWN
    };

    /**
     * Token type
     */
    // NOTE: Enums must match corresponding in jsmn.h
    enum Type
    {
        /// Number or boolean
        TYPE_PRIMITIVE = 0,
        /// Json object
        TYPE_OBJECT = 1,
        /// Json array
        TYPE_ARRAY = 2,
        /// String
        TYPE_STRING = 3
    };

    /**
     * Json node representation. Nodes
     * are in depth-first order with sibling
     * links for simplified traversal.
     *
     * NOTE: Siblings were added to support a read-only
     * lua-view of json-data. It's currently not used and
     * could potentially be removed for increased performance.
     */
    struct Node
    {
        /// Node type
        Type    m_Type;
        /// Start index inclusive into document json-data
        int     m_Start;
        /// End index exclusive into document json-data
        int     m_End;
        /// Size. Only applicable for arrays and objects
        int     m_Size;
        /// Sibling index
        int     m_Sibling;
    };

    /**
     * Json document
     */
    struct Document
    {
        /// Array of nodes. First node is root
        Node*  m_Nodes;
        /// Total number of nodes
        int    m_NodeCount;
        /// Json-data (unescaped)
        char* m_Json;
        /// User-data
        void*  m_UserData;
    };

    /**
     * Parse json data
     * @note The returned nodes index into document json-data.
     * @param buffer json buffer
     * @param doc document
     * @return RESULT_OK on success
     */
    Result Parse(const char* buffer, Document* doc);

    /**
     * Free json document nodes
     * @note The original json-data is not freed (const)
     * @param doc document
     */
    void   Free(Document* doc);

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
    const char* CStringArrayToJsonString(const char** array,
        unsigned int length);
}

#endif // #ifndef DM_JSON
