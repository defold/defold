// Generated, do not edit!
// Generated with cwd=/Users/agulev/projects/defold/engine/dlib and cmd=../../scripts/dmsdk/gen_sdk.py -i sdk_gen.json

// Copyright 2020-2026 The Defold Foundation
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

#ifndef DMSDK_JSON_GEN_HPP
#define DMSDK_JSON_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# JSON document API
 *
 * Strict JSON parsing and quoted string output. The document owns its text and
 * node storage, grows them on demand, and reuses them between parses. Callers
 * enforce input-size budgets; recursive parsing supports 64 levels.
 *
 * @document
 * @language C++
 * @name Json
 * @namespace dmJson
 * @path engine/dlib/src/dlib/json_gen.hpp
 */

#include <stdint.h>

#include <dlib/json.h>

namespace dmJson
{
    /*# JSON value type
     * JSON value type
     * @enum
     * @name Type
     * @language C++
     */
    typedef ::JsonType Type;
    static const Type TYPE_NULL = ::JSON_TYPE_NULL;
    static const Type TYPE_OBJECT = ::JSON_TYPE_OBJECT;
    static const Type TYPE_ARRAY = ::JSON_TYPE_ARRAY;
    static const Type TYPE_STRING = ::JSON_TYPE_STRING;
    static const Type TYPE_NUMBER = ::JSON_TYPE_NUMBER;
    static const Type TYPE_BOOL = ::JSON_TYPE_BOOL;

    /*# JSON value decoding policy
     * Preserve mode validates values without decoding them; their encoded spans
     * remain in JsonGetText. String and numeric accessors return their fallbacks.
     * Object keys are always decoded and must not contain embedded NUL.
     * @enum
     * @name ParseMode
     * @language C++
     */
    typedef ::JsonParseMode ParseMode;
    static const ParseMode PARSE_DECODE_VALUES = ::JSON_PARSE_DECODE_VALUES;
    static const ParseMode PARSE_PRESERVE_VALUES = ::JSON_PARSE_PRESERVE_VALUES;

    /*# JSON parsing result
     * Trailing data means a valid root value was followed by non-whitespace bytes.
     * @enum
     * @name Result
     * @language C++
     */
    typedef ::JsonResult Result;
    static const Result RESULT_OK = ::JSON_RESULT_OK;
    static const Result RESULT_SYNTAX_ERROR = ::JSON_RESULT_SYNTAX_ERROR;
    static const Result RESULT_TRAILING_DATA = ::JSON_RESULT_TRAILING_DATA;

    /*# Opaque, owned JSON document. Release with JsonDestroy.
     * Opaque, owned JSON document. Release with JsonDestroy.
     * @typedef
     * @name HDocument
     * @language C++
     */
    typedef HJsonDocument HDocument;

    /*# Direct children of an object or array
     * The first child index is -1 for an empty container.
     * @struct
     * @name Container
     * @language C++
     */
    typedef JsonContainer Container;

    /*# Read-only JSON node
     * Only the union member selected by m_Type is valid. Booleans use m_Number
     * (zero or one). Node zero is the root; m_Next is the next sibling or -1.
     * Offsets and lengths refer to the original input before in-place decoding.
     * @struct
     * @name Node
     * @language C++
     */
    typedef JsonNode Node;

    /*# JSON output callback. Consume or copy the supplied bytes before returning.
     * JSON output callback. Consume or copy the supplied bytes before returning.
     * @typedef
     * @name FWrite
     * @language C++
     * @param context [type:void*] Caller-supplied context
     * @param text [type:const char*] Bytes to write
     * @param size [type:uint32_t] Byte count
     */
    typedef FJsonWrite FWrite;

    /*# Create a reusable document. Zero capacity defers node allocation until parsing.
     * Create a reusable document. Zero capacity defers node allocation until parsing.
     * @name Create
     * @language C++
     * @param initial_capacity [type:uint32_t] Initial node capacity, at most UINT32_MAX / sizeof(JsonNode)
     * @return document [type:HDocument] Owned document
     */
    HDocument Create(uint32_t initial_capacity);

    /*# Destroy a document and all text and nodes it owns.
     * Destroy a document and all text and nodes it owns.
     * @name Destroy
     * @language C++
     * @param document [type:HDocument] Document to destroy
     */
    void Destroy(HDocument document);

    /*# Parse a bounded, read-only input buffer.
     * Every call invalidates previous node indices and borrowed pointers, including
     * on failure. Only inspect nodes and text after a successful parse. The input
     * is copied and need not remain alive after this call. Embedded NUL in decoded
     * strings, duplicate object keys, invalid UTF-8 and non-JSON syntax are rejected.
     * @name Parse
     * @language C++
     * @param document [type:HDocument] Document to reuse
     * @param text [type:const char*] Input bytes, not necessarily NUL-terminated
     * @param size [type:uint32_t] Input byte count
     * @param error_offset [type:uint32_t*] Optional offset where parsing stopped
     * @param mode [type:JsonParseMode] Value decoding policy
     * @return result [type:JsonResult] Parsing result
     */
    Result Parse(HDocument document, const char* text, uint32_t size, uint32_t* error_offset, ParseMode mode);

    /*# Get a node. An invalid index returns a shared null node with no children.
     * The returned pointer is borrowed until the next parse or document destruction.
     * @name GetNode
     * @language C++
     * @param document [type:HDocument] Document
     * @param node [type:int] Node index
     * @return value [type:const JsonNode*] Read-only node
     */
    const Node* GetNode(HDocument document, int node);

    /*# Get the number of nodes, including the root.
     * Get the number of nodes, including the root.
     * @name GetNodeCount
     * @language C++
     * @param document [type:HDocument] Document
     * @return count [type:uint32_t] Node count
     */
    uint32_t GetNodeCount(HDocument document);

    /*# Get the owned text buffer. Decoded strings and keys modify their source spans.
     * In preserve mode, value spans remain encoded. The returned pointer is borrowed
     * until the next parse or document destruction.
     * @name GetText
     * @language C++
     * @param document [type:HDocument] Document
     * @return text [type:const char*] Read-only text buffer
     */
    const char* GetText(HDocument document);

    /*# Get a decoded object key, or NULL for a node without a key.
     * The returned pointer is borrowed until the next parse or document destruction.
     * @name GetName
     * @language C++
     * @param document [type:HDocument] Document
     * @param node [type:int] Node index
     * @return name [type:const char*] Decoded key
     */
    const char* GetName(HDocument document, int node);

    /*# Find a direct object field by decoded name, or return -1 if not found.
     * Find a direct object field by decoded name, or return -1 if not found.
     * @name Field
     * @language C++
     * @param document [type:HDocument] Document
     * @param object [type:int] Object node index
     * @param name [type:const char*] NUL-terminated key
     * @return node [type:int] Field node index
     */
    int Field(HDocument document, int object, const char* name);

    /*# Get a decoded string, or the fallback for a missing or non-string value.
     * Preserve mode returns the fallback. Decoded strings are borrowed until the
     * next parse or document destruction.
     * @name String
     * @language C++
     * @param document [type:HDocument] Document
     * @param node [type:int] Node index
     * @param fallback [type:const char*] Fallback value
     * @return value [type:const char*] Decoded string or fallback
     */
    const char* String(HDocument document, int node, const char* fallback);

    /*# Get an exact integer in the int range, or the fallback.
     * Preserve mode returns the fallback.
     * @name Integer
     * @language C++
     * @param document [type:HDocument] Document
     * @param node [type:int] Node index
     * @param fallback [type:int] Fallback value
     * @return value [type:int] Integer or fallback
     */
    int Integer(HDocument document, int node, int fallback);

    /*# Get an exact unsigned 32-bit integer. Failure leaves the output unchanged.
     * Preserve mode returns false.
     * @name UnsignedInteger
     * @language C++
     * @param document [type:HDocument] Document
     * @param node [type:int] Node index
     * @param value [type:uint32_t*] Output value
     * @return success [type:bool] Whether an unsigned integer was read
     */
    bool UnsignedInteger(HDocument document, int node, uint32_t* value);

    /*# Get a boolean, or the fallback. Available in both parsing modes.
     * Get a boolean, or the fallback. Available in both parsing modes.
     * @name Boolean
     * @language C++
     * @param document [type:HDocument] Document
     * @param node [type:int] Node index
     * @param fallback [type:bool] Fallback value
     * @return value [type:bool] Boolean or fallback
     */
    bool Boolean(HDocument document, int node, bool fallback);

    /*# Emit a quoted JSON string. Invalid UTF-8 bytes are escaped as U+00xx.
     * Emit a quoted JSON string. Invalid UTF-8 bytes are escaped as U+00xx.
     * @name WriteString
     * @language C++
     * @param text [type:const char*] Input bytes
     * @param size [type:uint32_t] Input byte count
     * @param write [type:FWrite] Output callback
     * @param context [type:void*] Callback context
     */
    void WriteString(const char* text, uint32_t size, FWrite write, void* context);


} // namespace dmJson

#endif // #define DMSDK_JSON_GEN_HPP
/*# Opaque, owned JSON document. Release with JsonDestroy.
 * Opaque, owned JSON document. Release with JsonDestroy.
 * @typedef
 * @name HJsonDocument
 * @language C
 */

/*# JSON value type
 * JSON value type
 * @enum
 * @name JsonType
 * @language C
 */

/*# JSON value decoding policy
 * Preserve mode validates values without decoding them; their encoded spans
 * remain in JsonGetText. String and numeric accessors return their fallbacks.
 * Object keys are always decoded and must not contain embedded NUL.
 * @enum
 * @name JsonParseMode
 * @language C
 */

/*# JSON parsing result
 * Trailing data means a valid root value was followed by non-whitespace bytes.
 * @enum
 * @name JsonResult
 * @language C
 */

/*# Direct children of an object or array
 * The first child index is -1 for an empty container.
 * @struct
 * @name JsonContainer
 * @language C
 */

/*# Read-only JSON node
 * Only the union member selected by m_Type is valid. Booleans use m_Number
 * (zero or one). Node zero is the root; m_Next is the next sibling or -1.
 * Offsets and lengths refer to the original input before in-place decoding.
 * @struct
 * @name JsonNode
 * @language C
 */

/*# JSON output callback. Consume or copy the supplied bytes before returning.
 * JSON output callback. Consume or copy the supplied bytes before returning.
 * @typedef
 * @name FJsonWrite
 * @language C
 * @param context [type:void*] Caller-supplied context
 * @param text [type:const char*] Bytes to write
 * @param size [type:uint32_t] Byte count
 */

/*# Create a reusable document. Zero capacity defers node allocation until parsing.
 * Create a reusable document. Zero capacity defers node allocation until parsing.
 * @name JsonCreate
 * @language C
 * @param initial_capacity [type:uint32_t] Initial node capacity, at most UINT32_MAX / sizeof(JsonNode)
 * @return document [type:HJsonDocument] Owned document
 */

/*# Destroy a document and all text and nodes it owns.
 * Destroy a document and all text and nodes it owns.
 * @name JsonDestroy
 * @language C
 * @param document [type:HJsonDocument] Document to destroy
 */

/*# Parse a bounded, read-only input buffer.
 * Every call invalidates previous node indices and borrowed pointers, including
 * on failure. Only inspect nodes and text after a successful parse. The input
 * is copied and need not remain alive after this call. Embedded NUL in decoded
 * strings, duplicate object keys, invalid UTF-8 and non-JSON syntax are rejected.
 * @name JsonParse
 * @language C
 * @param document [type:HJsonDocument] Document to reuse
 * @param text [type:const char*] Input bytes, not necessarily NUL-terminated
 * @param size [type:uint32_t] Input byte count
 * @param error_offset [type:uint32_t*] Optional offset where parsing stopped
 * @param mode [type:JsonParseMode] Value decoding policy
 * @return result [type:JsonResult] Parsing result
 */

/*# Get a node. An invalid index returns a shared null node with no children.
 * The returned pointer is borrowed until the next parse or document destruction.
 * @name JsonGetNode
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @return value [type:const JsonNode*] Read-only node
 */

/*# Get the number of nodes, including the root.
 * Get the number of nodes, including the root.
 * @name JsonGetNodeCount
 * @language C
 * @param document [type:HJsonDocument] Document
 * @return count [type:uint32_t] Node count
 */

/*# Get the owned text buffer. Decoded strings and keys modify their source spans.
 * In preserve mode, value spans remain encoded. The returned pointer is borrowed
 * until the next parse or document destruction.
 * @name JsonGetText
 * @language C
 * @param document [type:HJsonDocument] Document
 * @return text [type:const char*] Read-only text buffer
 */

/*# Get a decoded object key, or NULL for a node without a key.
 * The returned pointer is borrowed until the next parse or document destruction.
 * @name JsonGetName
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @return name [type:const char*] Decoded key
 */

/*# Find a direct object field by decoded name, or return -1 if not found.
 * Find a direct object field by decoded name, or return -1 if not found.
 * @name JsonField
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param object [type:int] Object node index
 * @param name [type:const char*] NUL-terminated key
 * @return node [type:int] Field node index
 */

/*# Get a decoded string, or the fallback for a missing or non-string value.
 * Preserve mode returns the fallback. Decoded strings are borrowed until the
 * next parse or document destruction.
 * @name JsonString
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param fallback [type:const char*] Fallback value
 * @return value [type:const char*] Decoded string or fallback
 */

/*# Get an exact integer in the int range, or the fallback.
 * Preserve mode returns the fallback.
 * @name JsonInteger
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param fallback [type:int] Fallback value
 * @return value [type:int] Integer or fallback
 */

/*# Get an exact unsigned 32-bit integer. Failure leaves the output unchanged.
 * Preserve mode returns false.
 * @name JsonUnsignedInteger
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param value [type:uint32_t*] Output value
 * @return success [type:bool] Whether an unsigned integer was read
 */

/*# Get a boolean, or the fallback. Available in both parsing modes.
 * Get a boolean, or the fallback. Available in both parsing modes.
 * @name JsonBoolean
 * @language C
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param fallback [type:bool] Fallback value
 * @return value [type:bool] Boolean or fallback
 */

/*# Emit a quoted JSON string. Invalid UTF-8 bytes are escaped as U+00xx.
 * Emit a quoted JSON string. Invalid UTF-8 bytes are escaped as U+00xx.
 * @name JsonWriteString
 * @language C
 * @param text [type:const char*] Input bytes
 * @param size [type:uint32_t] Input byte count
 * @param write [type:FJsonWrite] Output callback
 * @param context [type:void*] Callback context
 */
