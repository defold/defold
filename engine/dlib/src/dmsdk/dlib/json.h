// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_JSON
#define DMSDK_JSON

/*# Json parsing functions
 *
 * API for platform independent parsing of json files
 *
 * @document
 * @name Json
 * @namespace dmJson
 * @path engine/dlib/src/dmsdk/dlib/json.h
 */

namespace dmJson
{
    /*# result enumeration
     *
     * Result enumeration.
     *
     * @enum
     * @name Result
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
     * @name Type
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
     * @name Node
     * @member m_Type [type:Type] Node type
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
     * @name Document
     * @member m_Nodes [type:Node*] Array of nodes. First node is root
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
     * @param length [type:uint32_t] The size of the json buffer (in bytes)
     * @param document [type:Document*] The output document
     * @return [type:Result] dmJson::RESULT_OK on success
     */
    Result Parse(const char* buffer, unsigned int buffer_length, Document* doc);

    /*# parse null terminated json data
     *
     * Parses a null terminated (utf-8) string into a dmJson::Document
     * The document must later be freed with dmJson::Free()
     *
     * @note Uses strlen() to figure out the length of the buffer
     * @note The returned nodes index into document json-data.
     *
     * @name Parse
     * @param buffer [type:const char*] The input data (Utf-8)
     * @param document [type:Document*] The output document
     * @return [type:Result] dmJson::RESULT_OK on success
     */
    Result Parse(const char* buffer, Document* doc);

    /*# deallocates json document
     *
     * Deallocates a previously created dmJson::Document
     *
     * @note The original json-data is not freed (const)
     *
     * @name Free
     * @param document [type:Document*] The document
     * @return [type:void]
     */
    void   Free(Document* doc);
}

#endif // #ifndef DMSDK_JSON
