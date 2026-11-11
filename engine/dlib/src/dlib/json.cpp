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

#include "json.hpp"
#include "json_private.hpp"
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/inline.h>
#include <dlib/utf8.h>
#include <simd/simd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "json_lexer.h"

using dmJson::Lexer;
using dmJson::PlainStringBlock;

// dmArray allocates with a uint32_t byte count. Node indices also fit in int.
static const uint32_t MAX_NODE_CAPACITY = UINT32_MAX / sizeof(JsonNode);
#if DM_ENDIAN == DM_ENDIAN_LITTLE && !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_NEON) && SIZE_MAX > UINT32_MAX && (defined(__GNUC__) || defined(__clang__))
static const uint32_t STRING_PADDING = 16;
#else
static const uint32_t STRING_PADDING = 0;
#endif

// The first byte is known to be ordinary text. Probe a short scalar prefix
// so escape-heavy strings do not repeatedly test overlapping SIMD blocks.
static uint32_t PlainStringBytes(const char* text, uint32_t size, bool ascii_only)
{
    uint32_t i = 1;
    for (; i < size && i < 8; ++i)
    {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c == '"' || c == '\\' || (ascii_only && c >= 0x80))
        {
            return i;
        }
    }
    while (size - i >= 16 && PlainStringBlock(text + i, ascii_only))
    {
        i += 16;
    }
    for (; i < size; ++i)
    {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c == '"' || c == '\\' || (ascii_only && c >= 0x80))
        {
            break;
        }
    }
    return i;
}

void JsonWriteString(const char* text, uint32_t size, FJsonWrite write, void* context)
{
    write(context, "\"", 1);
    uint32_t start = 0;
    for (uint32_t i = 0; i < size;)
    {
        unsigned char c = (unsigned char)text[i];
        uint32_t bytes = 0;
        if (c >= 0x80)
        {
            bytes = dmUtf8::ValidateSequence(text + i, size - i);
        }
        else if (c >= 32 && c != '"' && c != '\\')
        {
            i += PlainStringBytes(text + i, size - i, true);
            continue;
        }
        if (bytes)
        {
            i += bytes;
            continue;
        }
        if (i > start)
        {
            write(context, text + start, i - start);
        }
        const char hex[] = "0123456789abcdef";
        char escaped[] = { '\\', 'u', '0', '0', hex[c >> 4], hex[c & 15] };
        uint32_t length = sizeof(escaped);
        char short_escape = c == '"' || c == '\\' ? c :
                            c == '\b' ? 'b' : c == '\f' ? 'f' :
                            c == '\n' ? 'n' : c == '\r' ? 'r' : c == '\t' ? 't' : 0;
        if (short_escape)
        {
            escaped[1] = short_escape;
            length = 2;
        }
        write(context, escaped, length);
        start = ++i;
    }
    if (size > start)
    {
        write(context, text + start, size - start);
    }
    write(context, "\"", 1);
}

JsonDocument::JsonDocument(uint32_t initial_capacity)
: m_Text(0)
, m_Cursor(0)
, m_End(0)
, m_TextCapacity(0)
, m_Valid(true)
, m_DecodeValues(true)
{
    assert(initial_capacity <= MAX_NODE_CAPACITY);
    m_Nodes.SetCapacity(initial_capacity);
}
JsonDocument::~JsonDocument()
{
    free(m_Text);
}
static bool IsSpace(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

// Keep the common zero- and one-byte paths inline at each parser boundary.
static DM_FORCE_INLINE void Space(char*& cursor, const char* end)
{
    if (cursor == end || !IsSpace(*cursor))
    {
        return;
    }
    ++cursor;
    if (cursor == end || !IsSpace(*cursor))
    {
        return;
    }
    // Scan long indentation runs in whole words; finish mixed whitespace
    // and short tails one byte at a time.
    while (end - cursor >= 8)
    {
        uint64_t spaces;
        memcpy(&spaces, cursor, sizeof(spaces));
        if (spaces != UINT64_C(0x2020202020202020))
        {
            break;
        }
        cursor += 8;
    }
    while (cursor < end && IsSpace(*cursor))
    {
        ++cursor;
    }
}

// Test eight bounded bytes for quotes, backslashes and control characters.
static bool OrdinaryStringWord(uint64_t bytes)
{
    // Zero-byte tests mark quotes/backslashes; subtraction by 0x20 marks
    // control bytes. Borrow can mark later bytes but cannot hide a match.
    uint64_t ones = UINT64_C(0x0101010101010101);
    uint64_t quotes = bytes ^ UINT64_C(0x2222222222222222);
    uint64_t slashes = bytes ^ UINT64_C(0x5c5c5c5c5c5c5c5c);
    uint64_t special = ((quotes - ones) & ~quotes) | ((slashes - ones) & ~slashes);
    special |= (bytes - UINT64_C(0x2020202020202020)) & ~bytes;
    return !(special & UINT64_C(0x8080808080808080));
}

// Return the byte after an unescaped string, leaving cursor and text untouched
// on failure so the lexer can preserve decoding and exact error offsets.
// Parse pads the owned buffer with zeros on the native SIMD path, so a final
// full-width load always encounters a control byte before leaving storage.
static DM_FORCE_INLINE char* PlainStringEnd(char* cursor, const char* end)
{
    if (cursor == end || *cursor != '"')
    {
        return 0;
    }
    ++cursor;
#if DM_ENDIAN == DM_ENDIAN_LITTLE && !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_NEON) && SIZE_MAX > UINT32_MAX && (defined(__GNUC__) || defined(__clang__))
    while (true)
    {
        uint8x16_t bytes = vreinterpretq_u8_u32(SimdLoadUint(cursor));
        uint8x16_t special = vorrq_u8(vceqq_u8(bytes, vdupq_n_u8('"')),
                                      vorrq_u8(vceqq_u8(bytes, vdupq_n_u8('\\')), vcltq_u8(bytes, vdupq_n_u8(32))));
        // Pack the byte flags into four bits per byte. The first set nibble
        // locates the quote, escape or control byte that ends this fast scan.
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(special), 4)), 0);
        if (mask)
        {
            cursor += (uint32_t)__builtin_ctzll(mask) >> 2;
            return *cursor == '"' ? cursor + 1 : 0;
        }
        cursor += 16;
    }
#endif
    // Other backends use bounded words, then inspect the remaining bytes.
    while (end - cursor >= 8)
    {
        uint64_t bytes;
        memcpy(&bytes, cursor, sizeof(bytes));
        if (!OrdinaryStringWord(bytes))
        {
            break;
        }
        cursor += 8;
    }
    while (cursor < end)
    {
        uint8_t c = (uint8_t)*cursor++;
        if (c == '"')
        {
            return cursor;
        }
        if (c < 32 || c == '\\')
        {
            break;
        }
    }
    return 0;
}

// Read a value string, using the lexer for escapes and exact error offsets.
// Preserve mode validates and advances without storing a decoded value.
DM_FORCE_INLINE char* JsonDocument::ReadString(bool decode, char*& cursor, char* limit)
{
    char* end = PlainStringEnd(cursor, limit);
    if (end)
    {
        char* result = cursor + 1;
        cursor = end;
        if (decode)
        {
            end[-1] = 0;
        }
        return decode ? result : 0;
    }
    Lexer lexer(cursor, limit);
    char* result = lexer.ReadString(decode);
    cursor = lexer.m_Cursor;
    m_Valid = lexer.m_Valid;
    return decode ? result : 0;
}

// Parse one object or array. Specialization removes key handling from arrays.
// Keep the cursor local and publish it on every exit, including syntax errors.
template <bool object>
int JsonDocument::Container(int depth, int id, uint32_t offset, char*& outer_cursor, char* limit, char* text)
{
    char* cursor = outer_cursor;
    char closing_delimiter = object ? '}' : ']';
    ++cursor;
    Space(cursor, limit);
    int                previous_child = -1;
    uint32_t           child_count = 0;
    enum
    {
        SMALL_FIELD_COUNT = 64, SMALL_SLOT_COUNT = SMALL_FIELD_COUNT * 2
    };
    // Each occupied slot packs a decoded length (high 32 bits) and text offset
    // (low 32 bits). Text stays fixed even when the node array grows.
    uint64_t           name_entries[SMALL_SLOT_COUNT];
    uint64_t           occupied_slots[SMALL_SLOT_COUNT / 64] = {};
    // Allocate the wide-object index only on promotion; this call owns it.
    dmHashTable64<int>* field_index = 0;
    if (cursor < limit && *cursor == closing_delimiter)
    {
        ++cursor;
        m_Nodes[id].m_Length = (uint32_t)(cursor - text) - offset;
        outer_cursor = cursor;
        return id;
    }
    while (m_Valid && cursor < limit)
    {
        const char* name = 0;
        uint32_t    encoded_name_length = 0;
        uint64_t    name_hash = 0;
        bool        hash_indexed = false;
        if (object)
        {
            // Keys always decode, even in preserve mode. Keep their decoded
            // length for equality checks and their encoded length for spans.
            uint32_t decoded_length = 0;
            char* closing = PlainStringEnd(cursor, limit);
            if (closing)
            {
                name = cursor + 1;
                decoded_length = (uint32_t)(closing - name - 1);
                closing[-1] = 0;
                cursor = closing;
            }
            if (!name)
            {
                Lexer lexer(cursor, limit);
                name = lexer.ReadString(true);
                cursor = lexer.m_Cursor;
                m_Valid = lexer.m_Valid;
                decoded_length = lexer.m_StringLength;
            }
            if (!m_Valid)
            {
                break;
            }
            encoded_name_length = (uint32_t)(cursor - name - 1);
            Space(cursor, limit);
            if (!m_Valid || cursor == limit || *cursor++ != ':')
            {
                break;
            }
            // Most JSON objects are small. Index wider objects to avoid
            // scanning all previous fields for every decoded key.
            if (child_count == SMALL_FIELD_COUNT)
            {
                // Promote once, seeding from the completed children. Two
                // buckets per entry reduce collisions in wider objects.
                field_index = new dmHashTable64<int>;
                field_index->SetCapacity(SMALL_SLOT_COUNT * 2, SMALL_SLOT_COUNT);
                for (int field = m_Nodes[id].m_Container.m_First; field >= 0; field = m_Nodes[field].m_Next)
                {
                    const char* key = Name(field);
                    uint64_t    hash = dmHashBufferNoReverse64(key, (uint32_t)strlen(key));
                    if (!field_index->Get(hash))
                    {
                        field_index->Put(hash, field);
                    }
                }
            }
            if (field_index)
            {
                name_hash = dmHashBufferNoReverse64(name, decoded_length);
                int* first = field_index->Get(name_hash);
                hash_indexed = first != 0;
                // A hash collision must not reject a different key.
                // The first indexed field precedes every later key
                // with that hash in this object's sibling list.
                for (int field = first ? *first : -1; field >= 0; field = m_Nodes[field].m_Next)
                {
                    if (dmStrEq(Name(field), name))
                    {
                        m_Valid = false;
                        break;
                    }
                }
                if (!m_Valid)
                {
                    break;
                }
            }
            else
            {
                // The cheap hash only selects a starting slot. Compare length
                // and bytes for equality, then probe forward on collisions.
                // At most 64 entries occupy 128 slots, so a free slot exists.
                uint32_t length = decoded_length;
                uint32_t slot = (length ^ (uint8_t)name[0] ^ ((uint8_t)name[length ? length - 1 : 0] << 1)) & (SMALL_SLOT_COUNT - 1);
                uint64_t bit = UINT64_C(1) << (slot & 63);
                while (occupied_slots[slot >> 6] & bit)
                {
                    uint64_t entry = name_entries[slot];
                    if ((uint32_t)(entry >> 32) == length && !memcmp(text + (uint32_t)entry, name, length))
                    {
                        m_Valid = false;
                        break;
                    }
                    slot = (slot + 1) & (SMALL_SLOT_COUNT - 1);
                    bit = UINT64_C(1) << (slot & 63);
                }
                if (!m_Valid)
                {
                    break;
                }
                name_entries[slot] = ((uint64_t)length << 32) | (uint32_t)(name - text);
                occupied_slots[slot >> 6] |= bit;
            }
        }
        // Arrays already consumed whitespace after the opening delimiter or
        // comma. Object values also need to consume whitespace after the colon.
        if (object)
        {
            Space(cursor, limit);
        }
        int child = Value(depth + 1, cursor, limit, text);
        if (!m_Valid)
        {
            break;
        }
        // A nested value may reallocate nodes. Reacquire them by index before
        // assigning names or linking this child into its parent.
        if (object)
        {
            JsonNode& field = m_Nodes[child];
            field.m_NameOffset = (uint32_t)(name - text);
            field.m_NameLength = encoded_name_length;
        }
        if (previous_child < 0)
        {
            m_Nodes[id].m_Container.m_First = child;
        }
        else
        {
            m_Nodes[previous_child].m_Next = child;
        }
        previous_child = child;
        // Keep the first node for each hash; collision checks walk from it.
        if (field_index && !hash_indexed)
        {
            if (field_index->Full())
            {
                uint32_t capacity = field_index->Capacity();
                capacity = capacity > MAX_NODE_CAPACITY / 2 ? MAX_NODE_CAPACITY : capacity * 2;
                field_index->SetCapacity(capacity * 2, capacity);
            }
            field_index->Put(name_hash, child);
        }
        ++child_count;
        Space(cursor, limit);
        if (cursor == limit)
        {
            break;
        }
        if (*cursor == closing_delimiter)
        {
            // Finalize the direct-child count and original encoded span.
            JsonNode& container = m_Nodes[id];
            container.m_Container.m_Count = child_count;
            ++cursor;
            container.m_Length = (uint32_t)(cursor - text) - offset;
            outer_cursor = cursor;
            delete field_index;
            return id;
        }
        if (*cursor++ != ',')
        {
            break;
        }
        Space(cursor, limit);
    }
    // Preserve partial nodes and the failure position for error diagnostics.
    m_Valid = false;
    m_Nodes[id].m_Length = (uint32_t)(cursor - text) - offset;
    outer_cursor = cursor;
    delete field_index;
    return id;
}

// Accumulate at most nine digits in uint32_t, then convert exactly to double.
// Negatives (including -0), longer runs and decimal/exponent forms use the lexer.
// Leave cursor unchanged when that fallback is needed.
static DM_FORCE_INLINE bool SmallInteger(char*& cursor, const char* end, bool decode, double* number)
{
    char* position = cursor;
    if (*position < '0' || *position > '9')
    {
        return false;
    }
    uint32_t value = (uint32_t)(*position++ - '0');
    uint32_t digits = 1;
    // A leading zero must stand alone; a following digit requires the lexer.
    if (value)
    {
        while (position < end && *position >= '0' && *position <= '9' && digits < 9)
        {
            value = value * 10 + (uint32_t)(*position++ - '0');
            ++digits;
        }
    }
    // Do not accept a valid prefix of a longer JSON number.
    if (position < end && ((*position >= '0' && *position <= '9') || *position == '.' || *position == 'e' || *position == 'E'))
    {
        return false;
    }
    cursor = position;
    *number = decode ? (double)value : 0;
    return true;
}

// Parse a value after the caller has consumed its leading whitespace.
// Scalar readers stay inline; only containers recurse through Container().
DM_FORCE_INLINE int JsonDocument::Value(int depth, char*& cursor, char* limit, char* text)
{
    if (depth > 64 || cursor == limit || m_Nodes.Size() == MAX_NODE_CAPACITY)
    {
        m_Valid = false;
        return -1;
    }
    // Classify the leading byte once, before publishing the node. Invalid
    // starts use the boolean/literal path so failed parses retain their type.
    enum
    {
        T_NULL   = JSON_TYPE_NULL, T_OBJECT = JSON_TYPE_OBJECT, T_ARRAY = JSON_TYPE_ARRAY,
        T_STRING = JSON_TYPE_STRING, T_NUMBER = JSON_TYPE_NUMBER, T_BOOL = JSON_TYPE_BOOL
    };
    static const uint8_t types[256] = {
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_STRING, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_NUMBER, T_BOOL, T_BOOL,
        T_NUMBER, T_NUMBER, T_NUMBER, T_NUMBER, T_NUMBER, T_NUMBER, T_NUMBER, T_NUMBER,
        T_NUMBER, T_NUMBER, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_ARRAY, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_NULL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_OBJECT, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
        T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL, T_BOOL,
    };
    char first_byte = *cursor;
    JsonNode node = {};
    node.m_Type = (JsonType)types[(uint8_t)first_byte];
    node.m_Container.m_First = node.m_Next = -1;
    node.m_Offset = (uint32_t)(cursor - text);
    if (m_Nodes.Full())
    {
        uint32_t capacity = m_Nodes.Capacity();
        capacity = capacity > MAX_NODE_CAPACITY / 2 ? MAX_NODE_CAPACITY : capacity * 2;
        m_Nodes.SetCapacity(capacity ? capacity : 32);
    }
    // Append before descending so indices stay stable across node growth.
    int id = m_Nodes.Size();
    JsonNode* appended = m_Nodes.End();
    m_Nodes.Push(node);
    if (node.m_Type == JSON_TYPE_OBJECT)
    {
        return Container<true>(depth, id, node.m_Offset, cursor, limit, text);
    }
    else if (node.m_Type == JSON_TYPE_ARRAY)
    {
        return Container<false>(depth, id, node.m_Offset, cursor, limit, text);
    }
    // Scalar readers cannot append nodes, so this reference remains valid.
    JsonNode& value = *appended;
    if (node.m_Type == JSON_TYPE_STRING)
    {
        value.m_String = ReadString(m_DecodeValues, cursor, limit);
    }
    else if (node.m_Type == JSON_TYPE_NUMBER)
    {
        if (!SmallInteger(cursor, limit, m_DecodeValues, &value.m_Number))
        {
            Lexer lexer(cursor, limit);
            value.m_Number = lexer.ReadNumber(m_DecodeValues);
            cursor = lexer.m_Cursor;
            m_Valid = lexer.m_Valid;
        }
    }
    else
    {
        if (first_byte == 't' && limit - cursor >= 4 && !memcmp(cursor, "true", 4))
        {
            cursor += 4;
        }
        else if (first_byte == 'f' && limit - cursor >= 5 && !memcmp(cursor, "false", 5))
        {
            cursor += 5;
        }
        else if (first_byte == 'n' && limit - cursor >= 4 && !memcmp(cursor, "null", 4))
        {
            cursor += 4;
        }
        else
        {
            m_Valid = false;
        }
        value.m_Number = first_byte == 't';
    }
    value.m_Length = (uint32_t)(cursor - text) - node.m_Offset;
    return id;
}

// Validate input, parse an owned copy, and report the first unconsumed byte.
// Text/node allocations are retained for reuse between parses.
JsonResult JsonDocument::Parse(const char* text, uint32_t size, uint32_t* error_offset, JsonParseMode mode)
{
    m_Valid = false;
    m_DecodeValues = mode == JSON_PARSE_DECODE_VALUES;
    if (error_offset)
    {
        *error_offset = 0;
    }
    // The owned buffer needs one extra byte for its terminating NUL.
    if (size == UINT32_MAX)
    {
        return JSON_RESULT_SYNTAX_ERROR;
    }
    // Validate before changing document storage. String scans can then skip
    // ordinary UTF-8 bytes without validating each multibyte sequence again.
    uint32_t valid_bytes = dmUtf8::Validate(text, size);
    if (valid_bytes != size)
    {
        if (error_offset)
        {
            *error_offset = valid_bytes;
        }
        return JSON_RESULT_SYNTAX_ERROR;
    }
    // Logical capacity includes the terminating NUL, but excludes SIMD padding.
    m_Nodes.SetSize(0);
    if (size + 1 > m_TextCapacity)
    {
        uint32_t capacity = m_TextCapacity <= UINT32_MAX / 2 ? m_TextCapacity * 2 : size + 1;
        if (capacity < size + 1)
        {
            capacity = size + 1;
        }
        // Keep SIMD string loads within the owned allocation, including tails.
        char* text_buffer = (char*)realloc(m_Text, (size_t)capacity + STRING_PADDING);
        if (!text_buffer)
        {
            return JSON_RESULT_SYNTAX_ERROR;
        }
        m_Text = text_buffer;
        m_TextCapacity = capacity;
    }
    // Decode only in the owned copy. Zeros also stop the padded SIMD string
    // scanner at the logical end, including after reusing a larger buffer.
    memcpy(m_Text, text, size);
    memset(m_Text + size, 0, STRING_PADDING + 1);
    m_Cursor = m_Text;
    m_End = m_Text + size;
    m_Valid = true;
    // Read exactly one root value, then allow only trailing JSON whitespace.
    char* cursor = m_Cursor;
    Space(cursor, m_End);
    Value(0, cursor, m_End, m_Text);
    Space(cursor, m_End);
    m_Cursor = cursor;
    if (error_offset)
    {
        *error_offset = (uint32_t)(m_Cursor - m_Text);
    }
    return !m_Valid ? JSON_RESULT_SYNTAX_ERROR : m_Cursor == m_End ? JSON_RESULT_OK : JSON_RESULT_TRAILING_DATA;
}
const JsonNode& JsonDocument::Get(int node) const
{
    static const JsonNode missing = { JSON_TYPE_NULL, 0, { { -1, 0 } }, -1, 0, 0, 0 };
    return node >= 0 && node < (int)m_Nodes.Size() ? m_Nodes[node] : missing;
}
int JsonDocument::Field(int object, const char* name) const
{
    if (Get(object).m_Type != JSON_TYPE_OBJECT)
    {
        return -1;
    }
    for (int i = Get(object).m_Container.m_First; i >= 0; i = Get(i).m_Next)
    {
        if (Name(i) && dmStrEq(Name(i), name))
        {
            return i;
        }
    }
    return -1;
}
const char* JsonDocument::String(int node, const char* fallback) const
{
    return m_DecodeValues && Get(node).m_Type == JSON_TYPE_STRING ? Get(node).m_String : fallback;
}
int JsonDocument::Integer(int node, int fallback) const
{
    const JsonNode& value = Get(node);
    if (!m_DecodeValues || value.m_Type != JSON_TYPE_NUMBER)
    {
        return fallback;
    }
    double n = value.m_Number;
    return n >= INT_MIN && n <= INT_MAX && floor(n) == n ? (int)n : fallback;
}
bool JsonDocument::UnsignedInteger(int node, uint32_t* value) const
{
    const JsonNode& n = Get(node);
    if (!m_DecodeValues || n.m_Type != JSON_TYPE_NUMBER || n.m_Number < 0 || n.m_Number > UINT32_MAX || floor(n.m_Number) != n.m_Number)
    {
        return false;
    }
    *value = (uint32_t)n.m_Number;
    return true;
}
bool JsonDocument::Boolean(int node, bool fallback) const
{
    return Get(node).m_Type == JSON_TYPE_BOOL ? Get(node).m_Number != 0 : fallback;
}

const char* JsonDocument::Name(int node) const
{
    uint32_t offset = Get(node).m_NameOffset;
    return offset ? m_Text + offset : 0;
}

HJsonDocument JsonCreate(uint32_t initial_capacity)
{
    return new JsonDocument(initial_capacity);
}

void JsonDestroy(HJsonDocument document)
{
    delete document;
}

JsonResult JsonParse(HJsonDocument document, const char* text, uint32_t size, uint32_t* error_offset, JsonParseMode mode)
{
    return document->Parse(text, size, error_offset, mode);
}

const JsonNode* JsonGetNode(HJsonDocument document, int node)
{
    return &document->Get(node);
}

uint32_t JsonGetNodeCount(HJsonDocument document)
{
    return document->m_Nodes.Size();
}

const char* JsonGetText(HJsonDocument document)
{
    return document->m_Text;
}

const char* JsonGetName(HJsonDocument document, int node)
{
    return document->Name(node);
}

int JsonField(HJsonDocument document, int object, const char* name)
{
    return document->Field(object, name);
}

const char* JsonString(HJsonDocument document, int node, const char* fallback)
{
    return document->String(node, fallback);
}

int JsonInteger(HJsonDocument document, int node, int fallback)
{
    return document->Integer(node, fallback);
}

bool JsonUnsignedInteger(HJsonDocument document, int node, uint32_t* value)
{
    return document->UnsignedInteger(node, value);
}

bool JsonBoolean(HJsonDocument document, int node, bool fallback)
{
    return document->Boolean(node, fallback);
}

namespace dmJson
{
    HDocument Create(uint32_t initial_capacity)
    {
        return JsonCreate(initial_capacity);
    }
    void Destroy(HDocument document)
    {
        JsonDestroy(document);
    }
    Result Parse(HDocument document, const char* text, uint32_t size, uint32_t* error_offset, ParseMode mode)
    {
        return JsonParse(document, text, size, error_offset, mode);
    }
    const Node* GetNode(HDocument document, int node)
    {
        return JsonGetNode(document, node);
    }
    uint32_t GetNodeCount(HDocument document)
    {
        return JsonGetNodeCount(document);
    }
    const char* GetText(HDocument document)
    {
        return JsonGetText(document);
    }
    const char* GetName(HDocument document, int node)
    {
        return JsonGetName(document, node);
    }
    int Field(HDocument document, int object, const char* name)
    {
        return JsonField(document, object, name);
    }
    const char* String(HDocument document, int node, const char* fallback)
    {
        return JsonString(document, node, fallback);
    }
    int Integer(HDocument document, int node, int fallback)
    {
        return JsonInteger(document, node, fallback);
    }
    bool UnsignedInteger(HDocument document, int node, uint32_t* value)
    {
        return JsonUnsignedInteger(document, node, value);
    }
    bool Boolean(HDocument document, int node, bool fallback)
    {
        return JsonBoolean(document, node, fallback);
    }
    void WriteString(const char* text, uint32_t size, FWrite write, void* context)
    {
        JsonWriteString(text, size, write, context);
    }
}
