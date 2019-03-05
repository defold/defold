#ifndef DMSDK_JSON
#define DMSDK_JSON

/*# SDK Json Parser API documentation
 * [file:<dmsdk/dlib/json.h>]
 *
 * API for platform independent parsing of json files
 *
 * @document
 * @name Json
 * @namespace dmJson
 */

namespace dmJson
{
    /*# result enumeration
     *
     * Result enumeration.
     *
     * @enum
     * @name dmJson::Result
     * @member dmJson::RESULT_OK
     * @member dmJson::RESULT_SYNTAX_ERROR
     * @member dmJson::RESULT_INCOMPLETE
     * @member dmJson::RESULT_UNKNOWN
     *
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_SYNTAX_ERROR = -1,
        RESULT_INCOMPLETE = -2,
        RESULT_UNKNOWN = -1000,
    };

    /*# token type enumeration
     *
     * Token type enumeration.
     *
     * @enum
     * @name dmJson::Type
     * @member dmJson::TYPE_PRIMITIVE Number or boolean
     * @member dmJson::TYPE_OBJECT Json object
     * @member dmJson::TYPE_ARRAY Json array
     * @member dmJson::TYPE_STRING String
     *
     */
    enum Type
    {
        TYPE_PRIMITIVE = 0,
        TYPE_OBJECT = 1,
        TYPE_ARRAY = 2,
        TYPE_STRING = 3
    };

    /*# Json node representation.
     * Nodes are in depth-first order with sibling
     * links for simplified traversal.
     *
     * NOTE: Siblings were added to support a read-only
     * lua-view of json-data. It's currently not used and
     * could potentially be removed for increased performance.
     *
     * @struct
     * @name dmJson::Node
     * @member m_Type [type:dmJson::Type] Node type
     * @member m_Start [type:int] Start index inclusive into document json-data
     * @member m_End [type:int] End index exclusive into document json-data
     * @member m_Size [type:int] Size. Only applicable for arrays and objects
     * @member m_Sibling [type:int] Sibling index. -1 if no sibling
     */
    struct Node
    {
        Type    m_Type;
        int     m_Start;
        int     m_End;
        int     m_Size;
        int     m_Sibling;
    };

    /*# Json document
     * Holds a full json document
     *
     * @struct
     * @name dmJson::Document
     * @member m_Nodes [type:dmJson::Node] Array of nodes. First node is root
     * @member m_NodeCount [type:int] Total number of nodes
     * @member m_Json [type:char*] Json-data (unescaped)
     * @member m_UserData [type:void*] User-data
     */
    struct Document
    {
        Node*   m_Nodes;
        int     m_NodeCount;
        char*   m_Json;
        void*   m_UserData;
    };

    /*# parse json data
     *
     * Parses an (utf-8) string into a dmJson::Document
     * The document must later be freed with dmJson::Free()
     *
     * @note The returned nodes index into document json-data.
     *
     * @name Parse
     * @param buffer [type:const char*] The input data (Utf-8)
     * @param buffer_length [type:uint32_t] The size of the json buffer (in bytes)
     * @param document [type:dmJson::Document*] The output document
     * @return dmJson::RESULT_OK on success
     */
    Result Parse(const char* buffer, unsigned int buffer_length, Document* doc);

    /*# parse json data
     *
     * Parses a null terminated (utf-8) string into a dmJson::Document
     * The document must later be freed with dmJson::Free()
     *
     * @note Uses strlen() to figure out the length of the buffer
     * @note The returned nodes index into document json-data.
     *
     * @name Parse
     * @param buffer [type:const char*] The input data (Utf-8)
     * @param document [type:dmJson::Document*] The output document
     * @return dmJson::RESULT_OK on success
     */
    Result Parse(const char* buffer, Document* doc);

    /*# deallocates json document
     *
     * Deallocates a previously created dmJson::Document
     *
     * @note The original json-data is not freed (const)
     *
     * @name Free
     * @param document [type:dmJson::Document*] The document
     */
    void   Free(Document* doc);
}

#endif // #ifndef DMSDK_JSON
