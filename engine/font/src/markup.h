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

#ifndef DM_MARKUP_H
#define DM_MARKUP_H

#include <stdint.h>

/*# API for parsing tagged text markup
 *
 * Parses explicitly sized UTF-8 input into visible UTF-32 text, contiguous text
 * spans, generic tag attributes, and a persistent parent-linked style tree.
 * Parsing is syntactic: tag-specific values such as colors, sizes, gradients,
 * and effects are validated and resolved by later processing stages.
 *
 * Opening and closing tags must be strictly nested and use matching names.
 * Input ending inside a tag, entity, UTF-8 sequence, or unclosed tag hierarchy
 * produces `MARKUP_RESULT_INCOMPLETE` and no markup object.
 *
 * All pointers returned by this API are borrowed from the markup object and
 * remain valid until `MarkupDestroy()` is called.
 *
 * Link `font_richtext` for the parser and style/effect resolver. Engines that do
 * not need markup may link the API-compatible `font_richtext_null` library;
 * creation then returns `MARKUP_RESULT_UNSUPPORTED`, while plain text layout
 * and rendering remain available.
 *
 * @document
 * @name Markup
 * @language C
 */

/*#
 * A handle representing parsed markup.
 *
 * The handle owns a copy of the source text and all parsed arrays.
 *
 * @typedef
 * @name HMarkup
 */
typedef struct Markup* HMarkup;

/*# Invalid 16-bit array index
 *
 * Sentinel used where a parent or another 16-bit array index is absent.
 *
 * @constant
 * @name MARKUP_INVALID_INDEX
 */
static const uint16_t MARKUP_INVALID_INDEX = 0xffff;

/*# Markup parsing result
 *
 * @enum
 * @name MarkupResult
 * @member MARKUP_RESULT_OK Parsing succeeded and an output object was created.
 * @member MARKUP_RESULT_INCOMPLETE The input is a potentially valid prefix but ends before parsing can complete.
 * @member MARKUP_RESULT_SYNTAX_ERROR The input contains invalid markup syntax.
 * @member MARKUP_RESULT_INVALID_UTF8 The input contains an invalid UTF-8 sequence.
 * @member MARKUP_RESULT_LIMIT_EXCEEDED A 16-bit parser index or source-slice length limit was exceeded.
 * @member MARKUP_RESULT_UNSUPPORTED Markup support is not linked into this engine.
 */
enum MarkupResult
{
    MARKUP_RESULT_OK,
    MARKUP_RESULT_INCOMPLETE,
    MARKUP_RESULT_SYNTAX_ERROR,
    MARKUP_RESULT_INVALID_UTF8,
    MARKUP_RESULT_LIMIT_EXCEEDED,
    MARKUP_RESULT_UNSUPPORTED,
};

/*# Markup parsing error type
 *
 * Describes the first error encountered by `MarkupCreate()` or recovered by
 * `MarkupCreateRecovering()`.
 *
 * @enum
 * @name MarkupErrorType
 * @member MARKUP_ERROR_NONE No error occurred.
 * @member MARKUP_ERROR_INCOMPLETE_TAG Input ended inside a tag.
 * @member MARKUP_ERROR_INCOMPLETE_ENTITY Input ended inside an escaped entity.
 * @member MARKUP_ERROR_UNCLOSED_TAG Input ended with one or more open tags.
 * @member MARKUP_ERROR_INVALID_TAG A tag name or tag terminator is invalid.
 * @member MARKUP_ERROR_INVALID_ATTRIBUTE An attribute name, assignment, or value is invalid.
 * @member MARKUP_ERROR_INVALID_ENTITY An escaped entity is unknown or malformed.
 * @member MARKUP_ERROR_UNEXPECTED_CLOSING_TAG A closing tag was found without an active opening tag.
 * @member MARKUP_ERROR_MISMATCHED_CLOSING_TAG A closing tag does not match the innermost opening tag.
 * @member MARKUP_ERROR_INVALID_UTF8 The source contains an invalid or incomplete UTF-8 sequence.
 * @member MARKUP_ERROR_LIMIT_EXCEEDED A parser representation limit was exceeded.
 * @member MARKUP_ERROR_UNSUPPORTED Markup support is not linked into this engine.
 */
enum MarkupErrorType
{
    MARKUP_ERROR_NONE,
    MARKUP_ERROR_INCOMPLETE_TAG,
    MARKUP_ERROR_INCOMPLETE_ENTITY,
    MARKUP_ERROR_UNCLOSED_TAG,
    MARKUP_ERROR_INVALID_TAG,
    MARKUP_ERROR_INVALID_ATTRIBUTE,
    MARKUP_ERROR_INVALID_ENTITY,
    MARKUP_ERROR_UNEXPECTED_CLOSING_TAG,
    MARKUP_ERROR_MISMATCHED_CLOSING_TAG,
    MARKUP_ERROR_INVALID_UTF8,
    MARKUP_ERROR_LIMIT_EXCEEDED,
    MARKUP_ERROR_UNSUPPORTED,
};

/*# Markup parsing error
 *
 * Identifies the first parsing error and its location in the original source.
 *
 * @struct
 * @name MarkupError
 * @member m_ByteOffset [type: uint32_t] Zero-based UTF-8 byte offset in the original source.
 * @member m_Type [type: MarkupErrorType] Error classification.
 */
struct MarkupError
{
    uint32_t        m_ByteOffset;
    MarkupErrorType m_Type;
};

/*# Source text slice
 *
 * References text inside the source copy owned by `HMarkup`. Offsets and
 * lengths are measured in UTF-8 bytes, not Unicode codepoints.
 *
 * @struct
 * @name MarkupString
 * @member m_Offset [type: uint32_t] Zero-based byte offset from `MarkupGetSource()`.
 * @member m_Length [type: uint16_t] Slice length in bytes.
 */
struct MarkupString
{
    uint32_t m_Offset;
    uint16_t m_Length;
};

/*# Parsed markup attribute
 *
 * Represents a generic key/value pair. A zero-length `m_Name` identifies the
 * shorthand form, such as `red` in `<color=red>`.
 *
 * @struct
 * @name MarkupAttribute
 * @member m_Name [type: MarkupString] Attribute name, or an empty slice for a shorthand attribute.
 * @member m_Value [type: MarkupString] Attribute value without surrounding quotes.
 */
struct MarkupAttribute
{
    MarkupString m_Name;
    MarkupString m_Value;
};

/*# Markup style node
 *
 * Represents one opening tag in the persistent style tree. Node zero is the
 * synthetic root node. Every other node refers to its enclosing tag through
 * `m_Parent`. The attribute range indexes the array returned by
 * `MarkupGetAttributes()`.
 *
 * @struct
 * @name MarkupStyleNode
 * @member m_Tag [type: MarkupString] Opening-tag name. Empty for the root node.
 * @member m_Parent [type: uint16_t] Parent style-node index, or `MARKUP_INVALID_INDEX` for the root.
 * @member m_AttributeIndex [type: uint16_t] First attribute in the markup attribute array.
 * @member m_AttributeCount [type: uint16_t] Number of consecutive attributes belonging to this tag.
 * @member m_TextOffset [type: uint32_t] Visible UTF-32 text offset at the opening tag.
 * @member m_TextLength [type: uint32_t] Visible UTF-32 text length enclosed by the tag.
 */
struct MarkupStyleNode
{
    MarkupString m_Tag;
    uint16_t     m_Parent;
    uint16_t     m_AttributeIndex;
    uint16_t     m_AttributeCount;
    uint32_t     m_TextOffset;
    uint32_t     m_TextLength;
};

/*# Visible text span
 *
 * Describes a contiguous range in the stripped UTF-32 text returned by
 * `MarkupGetText()`. Every codepoint in the range has the same active innermost
 * style node. Enclosing styles are found by following the node's parent chain.
 *
 * Adjacent spans may refer to different nodes whose resolved render styles are
 * equal. Span identity is retained so span-relative effects can be resolved
 * independently.
 *
 * @struct
 * @name MarkupSpan
 * @member m_TextOffset [type: uint32_t] First UTF-32 codepoint index in the visible text array.
 * @member m_TextLength [type: uint32_t] Number of UTF-32 codepoints in the span.
 * @member m_StyleNodeIndex [type: uint16_t] Active style-node index for the span.
 */
struct MarkupSpan
{
    uint32_t m_TextOffset;
    uint32_t m_TextLength;
    uint16_t m_StyleNodeIndex;
};

/*# Parse markup text
 *
 * Parses exactly `text_length` bytes; the input does not need to be
 * null-terminated. On success, the returned markup object owns a source copy
 * and must be destroyed with `MarkupDestroy()`. On failure, `*out_markup` is
 * set to null. Empty input is valid and produces a markup object containing
 * only its synthetic root style node.
 *
 * `out_error` is optional. When supplied, it is reset to
 * `MARKUP_ERROR_NONE` before parsing and receives the first failure.
 * Returns `MARKUP_RESULT_UNSUPPORTED` when `font_richtext_null` is linked.
 *
 * @name MarkupCreate
 * @param text [type: const char*] UTF-8 source bytes. May be null only when `text_length` is zero.
 * @param text_length [type: uint32_t] Exact source length in bytes.
 * @param out_markup [type: HMarkup*] (out) Parsed markup handle. Must not be null.
 * @param out_error [type: MarkupError*] (out) Optional first-error information.
 * @return result [type: MarkupResult] Parsing result.
 */
MarkupResult MarkupCreate(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error);

/*# Parse markup text with error recovery
 *
 * Parses the same representation as `MarkupCreate()`, but preserves malformed
 * markup as visible literal text and resumes parsing at the next tag or entity.
 * This is intended for interactive tools and previews, where showing the
 * broken source while continuing to apply later valid markup makes authoring
 * errors easier to find.
 *
 * A mismatched closing tag rolls back the innermost unmatched opening tag and
 * its contents to literal text, then retries the closing tag against the next
 * enclosing tag. An unexpected or otherwise malformed tag is emitted as
 * literal text through its closing `>`. Invalid entities preserve their `&`
 * and parsing continues with the following byte. A syntactically valid but
 * unclosed tag remains active through the end of the text.
 *
 * Recoverable syntax and incomplete-input errors return `MARKUP_RESULT_OK`
 * with a valid markup object. When `out_error` is supplied, it receives the
 * first recovered error, which can be reported as a warning. Invalid UTF-8 and
 * representation-limit errors remain fatal and return no markup object.
 * Returns `MARKUP_RESULT_UNSUPPORTED` when `font_richtext_null` is linked.
 *
 * @name MarkupCreateRecovering
 * @param text [type: const char*] UTF-8 source bytes. May be null only when `text_length` is zero.
 * @param text_length [type: uint32_t] Exact source length in bytes.
 * @param out_markup [type: HMarkup*] (out) Parsed markup handle. Must not be null.
 * @param out_error [type: MarkupError*] (out) Optional first recovered or fatal error.
 * @return result [type: MarkupResult] Parsing result.
 */
MarkupResult MarkupCreateRecovering(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error);

/*# Parse a named-style markup fragment
 *
 * Parses one or more opening tags without visible text or closing tags. The
 * parser appends a synthetic codepoint so the regular style resolver can
 * compile definitions such as `<color=#336699><wave hz=2>`.
 *
 * Closing tags, self-closing tags, and non-whitespace text are rejected. On
 * success, the returned markup object must be destroyed with
 * `MarkupDestroy()`. Returns `MARKUP_RESULT_UNSUPPORTED` when
 * `font_richtext_null` is linked.
 *
 * @name MarkupCreateStyleFragment
 * @param text [type: const char*] UTF-8 style definition. May be null only when `text_length` is zero.
 * @param text_length [type: uint32_t] Exact definition length in bytes.
 * @param out_markup [type: HMarkup*] (out) Parsed markup handle. Must not be null.
 * @param out_error [type: MarkupError*] (out) Optional first-error information.
 * @return result [type: MarkupResult] Parsing result.
 */
MarkupResult MarkupCreateStyleFragment(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error);

/*# Destroy parsed markup
 *
 * Releases the source copy and all arrays owned by the handle. Passing null is
 * allowed. All pointers previously returned for the handle become invalid.
 *
 * @name MarkupDestroy
 * @param markup [type: HMarkup] Markup handle to destroy.
 */
void MarkupDestroy(HMarkup markup);

/*# Get the owned source copy
 *
 * Returns a copy of the original, unparsed markup input passed to
 * `MarkupCreate()` or `MarkupCreateRecovering()`. It therefore still contains
 * all tags, attribute syntax, escaped entities, and other original UTF-8
 * bytes. It is not the stripped visible text returned by `MarkupGetText()`.
 *
 * The source copy is null-terminated for convenience, while its authoritative
 * byte length is returned by `MarkupGetSourceLength()`. Embedded null bytes
 * from the original input remain part of the source and must not be treated as
 * its end.
 *
 * @name MarkupGetSource
 * @param markup [type: HMarkup] Valid markup handle.
 * @return source [type: const char*] Borrowed UTF-8 source copy.
 */
const char* MarkupGetSource(HMarkup markup);

/*# Get source length
 *
 * Returns the exact `text_length` value supplied to the successful
 * `MarkupCreate()` or `MarkupCreateRecovering()` call: the number of bytes in
 * the original, unparsed markup input. This includes bytes occupied by tags,
 * attributes, escaped entities, embedded nulls, and visible text. It excludes
 * only the convenience null terminator appended to the owned source copy.
 *
 * This value is unrelated to `MarkupGetTextLength()`, which reports the number
 * of UTF-32 codepoints remaining after tags are removed and entities decoded.
 *
 * @name MarkupGetSourceLength
 * @param markup [type: HMarkup] Valid markup handle.
 * @return length [type: uint32_t] Original unparsed markup input length in bytes.
 */
uint32_t MarkupGetSourceLength(HMarkup markup);

/*# Get visible text
 *
 * Returns the source text with tags removed and escaped entities decoded.
 * Self-closing `sprite` tags produce a U+FFFC OBJECT REPLACEMENT CHARACTER so
 * inline objects occupy a stable logical text position. The array is not
 * terminated. It may be null when the visible text is empty.
 *
 * @name MarkupGetText
 * @param markup [type: HMarkup] Valid markup handle.
 * @return text [type: const uint32_t*] Borrowed UTF-32 codepoint array.
 * @examples
 *
 * Tags are removed and entities are decoded in the returned text:
 *
 * ```c++
 * const char source[] = "A<color=red>B&amp;C</color>D";
 *
 * HMarkup markup = 0;
 * MarkupResult result = MarkupCreate(source, sizeof(source) - 1, &markup, 0);
 * if (result == MARKUP_RESULT_OK)
 * {
 *     const uint32_t* text = MarkupGetText(markup);
 *     uint32_t text_length = MarkupGetTextLength(markup);
 *
 *     // text contains the five UTF-32 codepoints: A B & C D
 *     // MarkupGetSourceLength(markup) is sizeof(source) - 1 because it
 *     // measures the original markup bytes, including tags and entities.
 *     assert(text_length == 5);
 *     assert(text[0] == 'A');
 *     assert(text[1] == 'B');
 *     assert(text[2] == '&');
 *     assert(text[3] == 'C');
 *     assert(text[4] == 'D');
 *
 *     MarkupDestroy(markup);
 * }
 * ```
 */
const uint32_t* MarkupGetText(HMarkup markup);

/*# Get visible text length
 *
 * @name MarkupGetTextLength
 * @param markup [type: HMarkup] Valid markup handle.
 * @return length [type: uint32_t] Number of UTF-32 codepoints in the visible text array.
 */
uint32_t MarkupGetTextLength(HMarkup markup);

/*# Get visible text spans
 *
 * The returned spans are ordered by `m_TextOffset`, do not overlap, and cover
 * all visible text. The pointer may be null when the span count is zero.
 *
 * @name MarkupGetSpans
 * @param markup [type: HMarkup] Valid markup handle.
 * @return spans [type: const MarkupSpan*] Borrowed span array.
 */
const MarkupSpan* MarkupGetSpans(HMarkup markup);

/*# Get visible text span count
 *
 * @name MarkupGetSpanCount
 * @param markup [type: HMarkup] Valid markup handle.
 * @return count [type: uint32_t] Number of visible text spans.
 */
uint32_t MarkupGetSpanCount(HMarkup markup);

/*# Get style nodes
 *
 * The array always contains the synthetic root at index zero. Parent indices
 * refer to earlier entries in the same array.
 *
 * @name MarkupGetStyleNodes
 * @param markup [type: HMarkup] Valid markup handle.
 * @return nodes [type: const MarkupStyleNode*] Borrowed style-node array.
 */
const MarkupStyleNode* MarkupGetStyleNodes(HMarkup markup);

/*# Get style-node count
 *
 * @name MarkupGetStyleNodeCount
 * @param markup [type: HMarkup] Valid markup handle.
 * @return count [type: uint32_t] Number of style nodes, including the synthetic root.
 */
uint32_t MarkupGetStyleNodeCount(HMarkup markup);

/*# Get parsed attributes
 *
 * Attributes are stored in tag order. Each style node identifies its
 * consecutive subrange using `m_AttributeIndex` and `m_AttributeCount`. The
 * pointer may be null when the attribute count is zero.
 *
 * @name MarkupGetAttributes
 * @param markup [type: HMarkup] Valid markup handle.
 * @return attributes [type: const MarkupAttribute*] Borrowed attribute array.
 */
const MarkupAttribute* MarkupGetAttributes(HMarkup markup);

/*# Get parsed attribute count
 *
 * @name MarkupGetAttributeCount
 * @param markup [type: HMarkup] Valid markup handle.
 * @return count [type: uint32_t] Number of parsed attributes.
 */
uint32_t MarkupGetAttributeCount(HMarkup markup);

#endif // DM_MARKUP_H
