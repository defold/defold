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

#ifndef DM_JSON_H
#define DM_JSON_H

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*# JSON document API
 * Strict JSON parsing and quoted string output. The document owns its text and
 * node storage, grows them on demand, and reuses them between parses. Callers
 * enforce input-size budgets; recursive parsing supports 64 levels.
 * @document
 * @name Json
 * @language C
 */

/*# Opaque, owned JSON document. Release with JsonDestroy.
 * @typedef
 * @name HJsonDocument
 */
typedef struct JsonDocument* HJsonDocument;

/*# JSON value type
 * @enum
 * @name JsonType
 */
typedef enum JsonType
{
    JSON_TYPE_NULL,
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_BOOL
} JsonType;

/*# JSON value decoding policy
 * Preserve mode validates values without decoding them; their encoded spans
 * remain in JsonGetText. String and numeric accessors return their fallbacks.
 * Object keys are always decoded and must not contain embedded NUL.
 * @enum
 * @name JsonParseMode
 */
typedef enum JsonParseMode
{
    JSON_PARSE_DECODE_VALUES,
    JSON_PARSE_PRESERVE_VALUES
} JsonParseMode;

/*# JSON parsing result
 * Trailing data means a valid root value was followed by non-whitespace bytes.
 * @enum
 * @name JsonResult
 */
typedef enum JsonResult
{
    JSON_RESULT_OK,
    JSON_RESULT_SYNTAX_ERROR,
    JSON_RESULT_TRAILING_DATA
} JsonResult;

/*# Direct children of an object or array
 * The first child index is -1 for an empty container.
 * @struct
 * @name JsonContainer
 */
typedef struct JsonContainer
{
    int      m_First;
    uint32_t m_Count;
} JsonContainer;

/*# Read-only JSON node
 * Only the union member selected by m_Type is valid. Booleans use m_Number
 * (zero or one). Node zero is the root; m_Next is the next sibling or -1.
 * Offsets and lengths refer to the original input before in-place decoding.
 * @struct
 * @name JsonNode
 */
typedef struct JsonNode
{
    JsonType m_Type;
    uint32_t m_NameOffset; // Zero means this node has no object key.
    union
    {
        JsonContainer m_Container;
        const char*   m_String;
        double        m_Number;
    };
    int      m_Next;
    uint32_t m_Offset;
    uint32_t m_Length;
    uint32_t m_NameLength; // Encoded field-name length, excluding quotes.
} JsonNode;

/*# JSON output callback. Consume or copy the supplied bytes before returning.
 * @typedef
 * @name FJsonWrite
 * @param context [type:void*] Caller-supplied context
 * @param text [type:const char*] Bytes to write
 * @param size [type:uint32_t] Byte count
 */
typedef void (*FJsonWrite)(void* context, const char* text, uint32_t size);

/*# Create a reusable document. Zero capacity defers node allocation until parsing.
 * @name JsonCreate
 * @param initial_capacity [type:uint32_t] Initial node capacity, at most UINT32_MAX / sizeof(JsonNode)
 * @return document [type:HJsonDocument] Owned document
 */
HJsonDocument JsonCreate(uint32_t initial_capacity);

/*# Destroy a document and all text and nodes it owns.
 * @name JsonDestroy
 * @param document [type:HJsonDocument] Document to destroy
 */
void JsonDestroy(HJsonDocument document);

/*# Parse a bounded, read-only input buffer.
 * Every call invalidates previous node indices and borrowed pointers, including
 * on failure. Only inspect nodes and text after a successful parse. The input
 * is copied and need not remain alive after this call. Embedded NUL in decoded
 * strings, duplicate object keys, invalid UTF-8 and non-JSON syntax are rejected.
 * @name JsonParse
 * @param document [type:HJsonDocument] Document to reuse
 * @param text [type:const char*] Input bytes, not necessarily NUL-terminated
 * @param size [type:uint32_t] Input byte count
 * @param error_offset [type:uint32_t*] Optional offset where parsing stopped
 * @param mode [type:JsonParseMode] Value decoding policy
 * @return result [type:JsonResult] Parsing result
 */
JsonResult JsonParse(HJsonDocument document, const char* text, uint32_t size, uint32_t* error_offset, JsonParseMode mode);

/*# Get a node. An invalid index returns a shared null node with no children.
 * The returned pointer is borrowed until the next parse or document destruction.
 * @name JsonGetNode
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @return value [type:const JsonNode*] Read-only node
 */
const JsonNode* JsonGetNode(HJsonDocument document, int node);

/*# Get the number of nodes, including the root.
 * @name JsonGetNodeCount
 * @param document [type:HJsonDocument] Document
 * @return count [type:uint32_t] Node count
 */
uint32_t JsonGetNodeCount(HJsonDocument document);

/*# Get the owned text buffer. Decoded strings and keys modify their source spans.
 * In preserve mode, value spans remain encoded. The returned pointer is borrowed
 * until the next parse or document destruction.
 * @name JsonGetText
 * @param document [type:HJsonDocument] Document
 * @return text [type:const char*] Read-only text buffer
 */
const char* JsonGetText(HJsonDocument document);

/*# Get a decoded object key, or NULL for a node without a key.
 * The returned pointer is borrowed until the next parse or document destruction.
 * @name JsonGetName
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @return name [type:const char*] Decoded key
 */
const char* JsonGetName(HJsonDocument document, int node);

/*# Find a direct object field by decoded name, or return -1 if not found.
 * @name JsonField
 * @param document [type:HJsonDocument] Document
 * @param object [type:int] Object node index
 * @param name [type:const char*] NUL-terminated key
 * @return node [type:int] Field node index
 */
int JsonField(HJsonDocument document, int object, const char* name);

/*# Get a decoded string, or the fallback for a missing or non-string value.
 * Preserve mode returns the fallback. Decoded strings are borrowed until the
 * next parse or document destruction.
 * @name JsonString
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param fallback [type:const char*] Fallback value
 * @return value [type:const char*] Decoded string or fallback
 */
const char* JsonString(HJsonDocument document, int node, const char* fallback);

/*# Get an exact integer in the int range, or the fallback.
 * Preserve mode returns the fallback.
 * @name JsonInteger
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param fallback [type:int] Fallback value
 * @return value [type:int] Integer or fallback
 */
int JsonInteger(HJsonDocument document, int node, int fallback);

/*# Get an exact unsigned 32-bit integer. Failure leaves the output unchanged.
 * Preserve mode returns false.
 * @name JsonUnsignedInteger
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param value [type:uint32_t*] Output value
 * @return success [type:bool] Whether an unsigned integer was read
 */
bool JsonUnsignedInteger(HJsonDocument document, int node, uint32_t* value);

/*# Get a boolean, or the fallback. Available in both parsing modes.
 * @name JsonBoolean
 * @param document [type:HJsonDocument] Document
 * @param node [type:int] Node index
 * @param fallback [type:bool] Fallback value
 * @return value [type:bool] Boolean or fallback
 */
bool JsonBoolean(HJsonDocument document, int node, bool fallback);

/*# Emit a quoted JSON string. Invalid UTF-8 bytes are escaped as U+00xx.
 * @name JsonWriteString
 * @param text [type:const char*] Input bytes
 * @param size [type:uint32_t] Input byte count
 * @param write [type:FJsonWrite] Output callback
 * @param context [type:void*] Callback context
 */
void JsonWriteString(const char* text, uint32_t size, FJsonWrite write, void* context);

#if defined(__cplusplus)
}
#endif

#endif // DM_JSON_H
