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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/array.h>

#include "font_outline.h"
#include "font_truetype.h"

#define FOURCC(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

// Values below are assigned by the OpenType and CFF specifications. Keeping
// them named makes the binary parsing code read in terms of the structures and
// operators it implements instead of their serialized numeric encodings.
enum FontTrueTypeCmapFormat
{
    CMAP_FORMAT_BYTE_ENCODING          = 0,
    CMAP_FORMAT_SEGMENT_MAPPING        = 4,
    CMAP_FORMAT_TRIMMED_TABLE          = 6,
    CMAP_FORMAT_SEGMENTED_COVERAGE     = 12,
    CMAP_FORMAT_MANY_TO_ONE_RANGE      = 13,
};

enum FontTrueTypeCmapPlatform
{
    CMAP_PLATFORM_UNICODE              = 0,
    CMAP_PLATFORM_WINDOWS              = 3,
    CMAP_WINDOWS_ENCODING_UNICODE_BMP  = 1,
    CMAP_WINDOWS_ENCODING_UNICODE_FULL = 10,
};

enum FontTrueTypeCFFVersion
{
    CFF_VERSION_1 = 1,
    CFF_VERSION_2 = 2,
};

enum FontTrueTypeCFFCharset
{
    CFF_CHARSET_ISO_ADOBE       = 0,
    CFF_CHARSET_EXPERT          = 1,
    CFF_CHARSET_EXPERT_SUBSET   = 2,
    CFF_CHARSET_CUSTOM          = 3,
    CFF_CHARSET_FORMAT_ARRAY    = 0,
    CFF_CHARSET_FORMAT_RANGE_8  = 1,
    CFF_CHARSET_FORMAT_RANGE_16 = 2,
};

enum FontTrueTypeCFFFDSelectFormat
{
    CFF_FDSELECT_FORMAT_ARRAY    = 0,
    CFF_FDSELECT_FORMAT_RANGE_16 = 3,
    CFF_FDSELECT_FORMAT_RANGE_32 = 4,
};

enum FontTrueTypeCFFDictOperator
{
    CFF_DICT_CHARSET      = 15,
    CFF_DICT_CHAR_STRINGS = 17,
    CFF_DICT_PRIVATE      = 18,
    CFF_DICT_SUBRS        = 19,
    CFF_DICT_VSINDEX      = 22,
    CFF_DICT_VSTORE       = 24,
    // Escaped DICT operators are represented internally as 0x100 | byte 2.
    CFF_DICT_FD_ARRAY     = 0x100 | 36,
    CFF_DICT_FD_SELECT    = 0x100 | 37,
};

enum FontTrueTypeCFFNumberEncoding
{
    CFF_NUMBER_SHORT_INT       = 28,
    CFF_NUMBER_LONG_INT        = 29,
    CFF_NUMBER_REAL            = 30,
    CFF_NUMBER_FIRST_COMPACT   = 32,
    CFF_NUMBER_LAST_COMPACT    = 246,
    CFF_NUMBER_FIRST_POSITIVE  = 247,
    CFF_NUMBER_LAST_POSITIVE   = 250,
    CFF_NUMBER_FIRST_NEGATIVE  = 251,
    CFF_NUMBER_LAST_NEGATIVE   = 254,
    CFF_NUMBER_FIXED_16_16     = 255,
};

enum FontTrueTypeType2Operator
{
    TYPE2_HSTEM       = 1,
    TYPE2_VSTEM       = 3,
    TYPE2_VMOVETO     = 4,
    TYPE2_RLINETO     = 5,
    TYPE2_HLINETO     = 6,
    TYPE2_VLINETO     = 7,
    TYPE2_RRCURVETO   = 8,
    TYPE2_CALLSUBR    = 10,
    TYPE2_RETURN      = 11,
    TYPE2_ESCAPE      = 12,
    TYPE2_ENDCHAR     = 14,
    TYPE2_VSINDEX     = 15,
    TYPE2_BLEND       = 16,
    TYPE2_HSTEMHM     = 18,
    TYPE2_HINTMASK    = 19,
    TYPE2_CNTRMASK    = 20,
    TYPE2_RMOVETO     = 21,
    TYPE2_HMOVETO     = 22,
    TYPE2_VSTEMHM     = 23,
    TYPE2_RCURVELINE  = 24,
    TYPE2_RLINECURVE  = 25,
    TYPE2_VVCURVETO   = 26,
    TYPE2_HHCURVETO   = 27,
    TYPE2_CALLGSUBR   = 29,
    TYPE2_VHCURVETO   = 30,
    TYPE2_HVCURVETO   = 31,
};

enum FontTrueTypeType2EscapedOperator
{
    TYPE2_HFLEX  = 34,
    TYPE2_FLEX   = 35,
    TYPE2_HFLEX1 = 36,
    TYPE2_FLEX1  = 37,
};

enum FontTrueTypeGlyfFlag
{
    GLYF_FLAG_ON_CURVE       = 0x01,
    GLYF_FLAG_X_SHORT        = 0x02,
    GLYF_FLAG_Y_SHORT        = 0x04,
    GLYF_FLAG_REPEAT         = 0x08,
    GLYF_FLAG_X_SAME         = 0x10,
    GLYF_FLAG_Y_SAME         = 0x20,
};

enum FontTrueTypeCompositeFlag
{
    GLYF_COMPOSITE_WORD_ARGUMENTS       = 0x0001,
    GLYF_COMPOSITE_ARGUMENTS_ARE_XY     = 0x0002,
    GLYF_COMPOSITE_UNIFORM_SCALE        = 0x0008,
    GLYF_COMPOSITE_MORE_COMPONENTS      = 0x0020,
    GLYF_COMPOSITE_SEPARATE_XY_SCALE    = 0x0040,
    GLYF_COMPOSITE_MATRIX                = 0x0080,
    GLYF_COMPOSITE_SCALED_OFFSET         = 0x0800,
    GLYF_COMPOSITE_UNSCALED_OFFSET       = 0x1000,
};

// Unicode range limits used by cmap validation and lookup.
static const uint32_t MAX_UNICODE_CODEPOINT = 0x10ffff;
static const uint32_t MAX_BMP_CODEPOINT = 0xffff;

// Format-defined Type 2 limits and defensive outline recursion limits.
static const uint32_t MAX_TYPE2_OPERANDS = 513;
static const uint32_t MAX_TYPE2_VARIATION_SCALARS = MAX_TYPE2_OPERANDS - 1;
static const uint32_t MAX_TYPE2_SUBROUTINE_DEPTH = 10;
static const uint32_t MAX_CFF_ENDCHAR_RECURSION = 32;
static const uint32_t MAX_GLYF_COMPOSITE_DEPTH = 32;

// Sizes and field offsets in the SFNT/TTC table layouts read by this parser.
static const uint32_t SFNT_VERSION_1_0 = 0x00010000;
static const uint32_t SFNT_OFFSET_TABLE_SIZE = 12;
static const uint32_t SFNT_TABLE_RECORD_SIZE = 16;
static const uint32_t TTC_HEADER_SIZE = 12;
static const uint32_t HEAD_REQUIRED_SIZE = 54;
static const uint32_t HEAD_UNITS_PER_EM_OFFSET = 18;
static const uint32_t HEAD_LOCA_FORMAT_OFFSET = 50;
static const uint32_t HHEA_REQUIRED_SIZE = 36;
static const uint32_t HHEA_ASCENT_OFFSET = 4;
static const uint32_t HHEA_DESCENT_OFFSET = 6;
static const uint32_t HHEA_LINE_GAP_OFFSET = 8;
static const uint32_t HHEA_METRIC_COUNT_OFFSET = 34;
static const uint32_t MAXP_REQUIRED_SIZE = 6;
static const uint32_t MAXP_GLYPH_COUNT_OFFSET = 4;

// Sizes and field offsets in the supported cmap subtable formats.
static const uint32_t CMAP_FORMAT_0_HEADER_SIZE = 6;
static const uint32_t CMAP_FORMAT_0_ENTRY_COUNT = 256;
static const uint32_t CMAP_FORMAT_4_HEADER_SIZE = 14;
static const uint32_t CMAP_FORMAT_4_MIN_SIZE = 16;
static const uint32_t CMAP_FORMAT_4_SEGMENT_COUNT_X2_OFFSET = 6;
static const uint32_t CMAP_FORMAT_6_HEADER_SIZE = 10;
static const uint32_t CMAP_FORMAT_6_FIRST_CODE_OFFSET = 6;
static const uint32_t CMAP_FORMAT_6_ENTRY_COUNT_OFFSET = 8;
static const uint32_t CMAP_FORMAT_12_HEADER_SIZE = 16;
static const uint32_t CMAP_FORMAT_12_GROUP_COUNT_OFFSET = 12;
static const uint32_t CMAP_FORMAT_12_GROUP_SIZE = 12;
static const uint32_t CMAP_GROUP_END_CODE_OFFSET = 4;
static const uint32_t CMAP_GROUP_START_GLYPH_OFFSET = 8;

// CFF INDEX, DICT, number, and predefined-charset constants.
static const uint32_t CFF_MAX_DICT_OPERANDS = 48;
static const uint32_t CFF_DICT_ESCAPE = 12;
static const uint32_t CFF_DICT_ESCAPED_OPERATOR_BASE = 0x100;
static const uint32_t CFF_REAL_END_NIBBLE = 15;
static const uint32_t CFF1_INDEX_COUNT_SIZE = 2;
static const uint32_t CFF2_INDEX_COUNT_SIZE = 4;
static const uint32_t CFF_MAX_INDEX_OFFSET_SIZE = 4;
static const uint32_t CFF_STANDARD_ENCODING_CODE_COUNT = 256;
static const uint32_t CFF_STANDARD_CHARSET_LAST_SID = 228;
static const int32_t  CFF_NUMBER_BYTE_RADIX = 256;
static const int32_t  CFF_NUMBER_COMPACT_ZERO = 139;
static const int32_t  CFF_NUMBER_TWO_BYTE_OFFSET = 108;

// glyf header layout, outline storage growth, and fixed-point scale.
static const uint32_t GLYF_HEADER_SIZE = 10;
static const uint32_t GLYF_X_MIN_OFFSET = 2;
static const uint32_t GLYF_Y_MIN_OFFSET = 4;
static const uint32_t GLYF_X_MAX_OFFSET = 6;
static const uint32_t GLYF_Y_MAX_OFFSET = 8;
static const uint32_t OUTLINE_ARRAY_GROWTH = 64;
static const float    F2DOT14_SCALE = 16384.0f;
static const uint32_t TYPE2_SMALL_SUBROUTINE_LIMIT = 1240;
static const uint32_t TYPE2_MEDIUM_SUBROUTINE_LIMIT = 33900;
static const int32_t  TYPE2_SMALL_SUBROUTINE_BIAS = 107;
static const int32_t  TYPE2_MEDIUM_SUBROUTINE_BIAS = 1131;
static const int32_t  TYPE2_LARGE_SUBROUTINE_BIAS = 32768;

enum FontTrueTypeLocaFormat
{
    LOCA_FORMAT_SHORT = 0,
    LOCA_FORMAT_LONG  = 1,
};

// This module parses SFNT font and collection data. FontTrueType borrows one
// immutable font file and retains only parsed offsets and CFF dictionaries.
// Glyph requests decode glyf or CFF CharStrings on demand into absolute,
// unscaled font coordinates. Variation blends are evaluated at the default
// normalized coordinates; hint operators affect mask parsing but are
// intentionally not applied to the geometric outline.
//
// Implementation references:
// - The OpenType specification describes the SFNT table directory, TrueType
//   Collections, and the common cmap/head/hhea/hmtx/maxp/loca tables:
//   https://learn.microsoft.com/en-us/typography/opentype/spec/otff
//   https://learn.microsoft.com/en-us/typography/opentype/spec/ttch01
//   https://learn.microsoft.com/en-us/typography/opentype/spec/cmap
//   https://learn.microsoft.com/en-us/typography/opentype/spec/head
//   https://learn.microsoft.com/en-us/typography/opentype/spec/hhea
//   https://learn.microsoft.com/en-us/typography/opentype/spec/hmtx
//   https://learn.microsoft.com/en-us/typography/opentype/spec/maxp
//   https://learn.microsoft.com/en-us/typography/opentype/spec/loca
// - The OpenType glyf and CFF specifications are the normative references for
//   TrueType contours, composites, DICT, INDEX, CharString, and FDSelect:
//   https://learn.microsoft.com/en-us/typography/opentype/spec/glyf
//   https://learn.microsoft.com/en-us/typography/opentype/spec/cff
//   https://learn.microsoft.com/en-us/typography/opentype/spec/cff2
// - stb_truetype's stbtt__run_charstring() informed the compact iterative
//   Type 2 operator dispatch, subroutine stack, and expansion of curve and flex
//   operators used by both CFF1 and CFF2:
//   engine/font/src/test/stb_truetype.h
// - HarfBuzz's CFF interpreter was used as a reference for separating parsed,
//   immutable CFF state from per-glyph interpretation, and for cross-checking
//   FDSelect, subroutine bias, PrivateDICT, vsindex, and blend handling:
//   external/harfbuzz/package/harfbuzz-13.2.1/src/hb-ot-cff2-table.hh
//   external/harfbuzz/package/harfbuzz-13.2.1/src/hb-cff-interp-cs-common.hh
//
// These references are compile-time guidance only; outline extraction does not
// introduce a HarfBuzz runtime dependency.

struct FontTrueTypeBuffer
{
    // Borrowed bounded view used while reading CFF data.
    const uint8_t* m_Data;
    uint32_t       m_Size;
    uint32_t       m_Position;
};

struct FontTrueTypeIndex
{
    // INDEX offsets remain encoded in the source table and are read on demand.
    const uint8_t* m_Data;
    uint32_t       m_Size;
    uint32_t       m_Count;
    uint32_t       m_Offsets;
    uint32_t       m_Objects;
    uint8_t        m_OffsetSize;
};

struct FontTrueTypePrivate
{
    // Per-FontDICT state needed while executing a glyph CharString.
    FontTrueTypeIndex m_Subrs;
    uint32_t          m_VSIndex;
};

struct FontTrueType
{
    const uint8_t*               m_FontData;           // Borrowed complete SFNT or TTC data.
    const uint8_t*               m_Data;               // Borrowed CFF/CFF2 table data; null for glyf.
    uint32_t                     m_DataSize;           // Size of m_Data in bytes.
    FontTrueTypeIndex            m_GlobalSubrs;        // CFF global subroutine INDEX.
    FontTrueTypeIndex            m_CharStrings;        // CFF glyph CharStrings INDEX.
    FontTrueTypeIndex            m_FontDicts;          // CFF CID Font DICT INDEX.
    const uint8_t*               m_Charset;            // Custom CFF charset data; null for predefined charsets.
    uint32_t                     m_CharsetSize;        // Available bytes at m_Charset.
    uint32_t                     m_CharsetID;          // CFF predefined charset ID, or 3 for custom data.
    const uint8_t*               m_FDSelect;           // CFF glyph-to-Font-DICT mapping.
    uint32_t                     m_FDSelectSize;       // Available bytes at m_FDSelect.
    const uint8_t*               m_VariationStore;     // CFF2 ItemVariationStore data.
    uint32_t                     m_VariationStoreSize; // Size of m_VariationStore in bytes.
    dmArray<FontTrueTypePrivate> m_Private;            // Private DICT state for each CFF Font DICT.
    uint32_t                     m_CmapOffset;         // Selected cmap subtable offset in m_FontData.
    uint32_t                     m_CmapSize;           // Size of the selected cmap subtable.
    uint32_t                     m_HmtxOffset;         // hmtx table offset in m_FontData.
    uint32_t                     m_LocaOffset;         // loca table offset in m_FontData.
    uint32_t                     m_LocaSize;           // Size of the loca table.
    uint32_t                     m_GlyfOffset;         // glyf table offset in m_FontData.
    uint32_t                     m_GlyfSize;           // Size of the glyf table.
    uint32_t                     m_NumGlyphs;          // Glyph count read from maxp.
    uint32_t                     m_NumHMetrics;        // Long horizontal metric count from hhea.
    uint32_t                     m_UnitsPerEm;         // Design units per em read from head.
    int32_t                      m_Ascent;             // Typographic ascent read from hhea.
    int32_t                      m_Descent;            // Typographic descent read from hhea.
    int32_t                      m_LineGap;            // Typographic line gap read from hhea.
    int16_t                      m_IndexToLocFormat;   // loca entry format read from head.
    FontOutlineType              m_OutlineType;        // Selected glyf, CFF1, or CFF2 decoder.
};

static uint16_t ReadU16(const uint8_t* data)
{
    return ((uint16_t)data[0] << 8) | data[1];
}

static int16_t ReadS16(const uint8_t* data)
{
    return (int16_t)ReadU16(data);
}

static uint32_t ReadU32(const uint8_t* data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
}

static bool IsSFNT(const uint8_t* data, uint32_t data_size, uint32_t offset)
{
    if (offset > data_size || data_size - offset < SFNT_OFFSET_TABLE_SIZE)
        return false;
    uint32_t signature = ReadU32(data + offset);
    return signature == SFNT_VERSION_1_0 || signature == FOURCC('O', 'T', 'T', 'O') ||
           signature == FOURCC('t', 'r', 'u', 'e') || signature == FOURCC('t', 'y', 'p', '1');
}

uint32_t FontTrueTypeGetFaceCount(const void* source, uint32_t data_size)
{
    // A standalone SFNT has one face. A TTC stores absolute offsets to each
    // embedded SFNT directory.
    const uint8_t* data = (const uint8_t*)source;
    if (!data || data_size < TTC_HEADER_SIZE)
        return 0;
    if (ReadU32(data) != FOURCC('t', 't', 'c', 'f'))
        return IsSFNT(data, data_size, 0) ? 1 : 0;
    uint32_t count = ReadU32(data + 8);
    if (count > (data_size - TTC_HEADER_SIZE) / 4)
        return 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (!IsSFNT(data, data_size, ReadU32(data + TTC_HEADER_SIZE + i * 4)))
            return 0;
    }
    return count;
}

static bool GetFaceOffset(const uint8_t* data, uint32_t data_size, uint32_t face_index, uint32_t* font_offset)
{
    uint32_t face_count = FontTrueTypeGetFaceCount(data, data_size);
    if (face_index >= face_count)
        return false;
    if (face_count == 1 && ReadU32(data) != FOURCC('t', 't', 'c', 'f'))
        *font_offset = 0;
    else
        *font_offset = ReadU32(data + TTC_HEADER_SIZE + face_index * 4);
    return true;
}

static bool FindTable(const uint8_t* data, uint32_t data_size, uint32_t font_offset, uint32_t tag, uint32_t* table_offset, uint32_t* table_size)
{
    if (font_offset > data_size || data_size - font_offset < SFNT_OFFSET_TABLE_SIZE)
        return false;
    uint16_t table_count = ReadU16(data + font_offset + 4);
    uint32_t records = font_offset + SFNT_OFFSET_TABLE_SIZE;
    if (table_count > (data_size - records) / SFNT_TABLE_RECORD_SIZE)
        return false;
    for (uint16_t i = 0; i < table_count; ++i)
    {
        const uint8_t* record = data + records + i * SFNT_TABLE_RECORD_SIZE;
        if (ReadU32(record) != tag)
            continue;
        uint32_t offset = ReadU32(record + 8);
        uint32_t size = ReadU32(record + 12);
        if (offset > data_size || size > data_size - offset)
            return false;
        *table_offset = offset;
        *table_size = size;
        return true;
    }
    return false;
}

static bool ValidateCmap(const FontTrueType* font, const uint8_t* cmap, uint32_t size)
{
    uint16_t format = ReadU16(cmap);
    if (format == CMAP_FORMAT_BYTE_ENCODING)
    {
        if (size < CMAP_FORMAT_0_HEADER_SIZE + CMAP_FORMAT_0_ENTRY_COUNT)
            return false;
        for (uint32_t i = 0; i < CMAP_FORMAT_0_ENTRY_COUNT; ++i)
            if (cmap[CMAP_FORMAT_0_HEADER_SIZE + i] >= font->m_NumGlyphs)
                return false;
        return true;
    }
    if (format == CMAP_FORMAT_TRIMMED_TABLE)
    {
        if (size < CMAP_FORMAT_6_HEADER_SIZE)
            return false;
        uint32_t count = ReadU16(cmap + CMAP_FORMAT_6_ENTRY_COUNT_OFFSET);
        if (count > (size - CMAP_FORMAT_6_HEADER_SIZE) / 2)
            return false;
        for (uint32_t i = 0; i < count; ++i)
            if (ReadU16(cmap + CMAP_FORMAT_6_HEADER_SIZE + i * 2) >= font->m_NumGlyphs)
                return false;
        return true;
    }
    if (format == CMAP_FORMAT_SEGMENTED_COVERAGE || format == CMAP_FORMAT_MANY_TO_ONE_RANGE)
    {
        if (size < CMAP_FORMAT_12_HEADER_SIZE)
            return false;
        uint32_t count = ReadU32(cmap + CMAP_FORMAT_12_GROUP_COUNT_OFFSET);
        if (count > (size - CMAP_FORMAT_12_HEADER_SIZE) / CMAP_FORMAT_12_GROUP_SIZE)
            return false;
        uint32_t previous_end = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint8_t* group = cmap + CMAP_FORMAT_12_HEADER_SIZE + i * CMAP_FORMAT_12_GROUP_SIZE;
            uint32_t       first = ReadU32(group);
            uint32_t       last = ReadU32(group + CMAP_GROUP_END_CODE_OFFSET);
            uint32_t       glyph = ReadU32(group + CMAP_GROUP_START_GLYPH_OFFSET);
            if (first > last || last > MAX_UNICODE_CODEPOINT || (i != 0 && first <= previous_end))
                return false;
            if (format == CMAP_FORMAT_SEGMENTED_COVERAGE)
            {
                uint32_t glyph_count = last - first;
                if (glyph >= font->m_NumGlyphs || glyph_count > font->m_NumGlyphs - glyph - 1)
                    return false;
            }
            else if (glyph >= font->m_NumGlyphs)
            {
                return false;
            }
            previous_end = last;
        }
        return true;
    }
    if (format != CMAP_FORMAT_SEGMENT_MAPPING || size < CMAP_FORMAT_4_MIN_SIZE)
        return false;

    uint32_t segment_count_x2 = ReadU16(cmap + CMAP_FORMAT_4_SEGMENT_COUNT_X2_OFFSET);
    if (segment_count_x2 == 0 || (segment_count_x2 & 1) != 0)
        return false;
    uint32_t segment_count = segment_count_x2 / 2;
    uint32_t end_codes = CMAP_FORMAT_4_HEADER_SIZE;
    uint32_t start_codes = end_codes + segment_count * 2 + 2;
    uint32_t deltas = start_codes + segment_count * 2;
    uint32_t range_offsets = deltas + segment_count * 2;
    if (range_offsets + segment_count * 2 > size)
        return false;
    uint32_t previous_end = 0;
    for (uint32_t i = 0; i < segment_count; ++i)
    {
        uint32_t start = ReadU16(cmap + start_codes + i * 2);
        uint32_t end = ReadU16(cmap + end_codes + i * 2);
        int32_t  delta = ReadS16(cmap + deltas + i * 2);
        uint32_t range_offset = ReadU16(cmap + range_offsets + i * 2);
        if (start > end || (i != 0 && start <= previous_end))
            return false;
        for (uint32_t codepoint = start; codepoint <= end; ++codepoint)
        {
            uint32_t glyph;
            if (range_offset == 0)
            {
                glyph = (codepoint + delta) & MAX_BMP_CODEPOINT;
            }
            else
            {
                uint32_t address = range_offsets + i * 2 + range_offset + (codepoint - start) * 2;
                if (address + 2 > size)
                    return false;
                glyph = ReadU16(cmap + address);
                if (glyph)
                    glyph = (glyph + delta) & MAX_BMP_CODEPOINT;
            }
            if (glyph >= font->m_NumGlyphs)
                return false;
        }
        previous_end = end;
    }
    return previous_end == MAX_BMP_CODEPOINT;
}

static bool SelectCmap(FontTrueType* font, uint32_t table_offset, uint32_t table_size)
{
    // Prefer full Unicode and BMP mappings over last-resort format 13 mappings.
    // For equal formats, prefer the Unicode platform followed by Windows
    // Unicode encodings.
    const uint8_t* data = font->m_FontData;
    if (table_size < 4)
        return false;
    uint16_t count = ReadU16(data + table_offset + 2);
    if (count > (table_size - 4) / 8)
        return false;
    uint32_t selected_offset = 0;
    uint32_t selected_size = 0;
    uint32_t selected_priority = 0;
    for (uint16_t i = 0; i < count; ++i)
    {
        const uint8_t* record = data + table_offset + 4 + i * 8;
        uint16_t       platform = ReadU16(record);
        uint16_t       encoding = ReadU16(record + 2);
        uint32_t       encoding_priority = 0;
        if (platform == CMAP_PLATFORM_UNICODE)
            encoding_priority = 3;
        else if (platform == CMAP_PLATFORM_WINDOWS && encoding == CMAP_WINDOWS_ENCODING_UNICODE_FULL)
            encoding_priority = 2;
        else if (platform == CMAP_PLATFORM_WINDOWS && encoding == CMAP_WINDOWS_ENCODING_UNICODE_BMP)
            encoding_priority = 1;
        if (encoding_priority == 0)
            continue;
        uint32_t       relative = ReadU32(record + 4);
        if (relative > table_size || table_size - relative < 4)
            continue;
        const uint8_t* subtable = data + table_offset + relative;
        uint16_t       format = ReadU16(subtable);
        uint32_t       size = 0;
        if (format == CMAP_FORMAT_SEGMENTED_COVERAGE || format == CMAP_FORMAT_MANY_TO_ONE_RANGE)
        {
            if (table_size - relative >= 8)
                size = ReadU32(subtable + 4);
        }
        else
        {
            size = ReadU16(subtable + 2);
        }
        if (size < 4 || size > table_size - relative)
            continue;
        if (!ValidateCmap(font, subtable, size))
            continue;
        uint32_t format_priority = 0;
        if (format == CMAP_FORMAT_SEGMENTED_COVERAGE)
            format_priority = 4;
        else if (format == CMAP_FORMAT_SEGMENT_MAPPING)
            format_priority = 3;
        else if (format == CMAP_FORMAT_MANY_TO_ONE_RANGE)
            format_priority = 2;
        else if (format == CMAP_FORMAT_TRIMMED_TABLE || format == CMAP_FORMAT_BYTE_ENCODING)
            format_priority = 1;
        uint32_t priority = format_priority * 4 + encoding_priority;
        if (priority > selected_priority)
        {
            selected_offset = table_offset + relative;
            selected_size = size;
            selected_priority = priority;
        }
    }
    font->m_CmapOffset = selected_offset;
    font->m_CmapSize = selected_size;
    return selected_priority != 0;
}

static uint32_t ReadOffset(const uint8_t* data, uint8_t size)
{
    uint32_t value = 0;
    for (uint8_t i = 0; i < size; ++i)
        value = (value << 8) | data[i];
    return value;
}

static bool BufferReadU8(FontTrueTypeBuffer* buffer, uint8_t* value)
{
    if (buffer->m_Position == buffer->m_Size)
        return false;
    *value = buffer->m_Data[buffer->m_Position++];
    return true;
}

static bool BufferReadU16(FontTrueTypeBuffer* buffer, uint16_t* value)
{
    if (buffer->m_Size - buffer->m_Position < 2)
        return false;
    *value = ReadU16(buffer->m_Data + buffer->m_Position);
    buffer->m_Position += 2;
    return true;
}

static bool BufferReadU32(FontTrueTypeBuffer* buffer, uint32_t* value)
{
    if (buffer->m_Size - buffer->m_Position < 4)
        return false;
    *value = ReadU32(buffer->m_Data + buffer->m_Position);
    buffer->m_Position += 4;
    return true;
}

static bool ParseIndex(const uint8_t* data, uint32_t data_size, uint32_t offset, uint32_t count_size, FontTrueTypeIndex* index, uint32_t* end)
{
    // CFF1 encodes INDEX counts in two bytes; CFF2 uses four. Item offsets are
    // one-based and share the INDEX-selected offSize.
    if (offset > data_size || (count_size != CFF1_INDEX_COUNT_SIZE && count_size != CFF2_INDEX_COUNT_SIZE) || data_size - offset < count_size)
        return false;
    uint32_t count = count_size == CFF1_INDEX_COUNT_SIZE ? ReadU16(data + offset) : ReadU32(data + offset);
    memset(index, 0, sizeof(*index));
    index->m_Data = data;
    index->m_Size = data_size;
    index->m_Count = count;
    index->m_Offsets = offset + count_size + 1;
    if (count == 0)
    {
        *end = offset + count_size;
        return true;
    }
    if (data_size - offset < count_size + 1)
        return false;
    uint8_t  offset_size = data[offset + count_size];
    uint32_t available = data_size - offset - count_size - 1;
    if (offset_size == 0 || offset_size > CFF_MAX_INDEX_OFFSET_SIZE || count == UINT32_MAX || count + 1 > available / offset_size)
        return false;
    uint32_t offsets_size = (count + 1) * offset_size;
    uint32_t objects = offset + count_size + 1 + offsets_size;
    uint32_t last = ReadOffset(data + offset + count_size + 1 + count * offset_size, offset_size);
    if (last == 0 || last - 1 > data_size - objects)
        return false;
    index->m_OffsetSize = offset_size;
    index->m_Objects = objects;
    *end = objects + last - 1;
    return true;
}

static bool IndexGet(const FontTrueTypeIndex* index, uint32_t item, FontTrueTypeBuffer* buffer)
{
    if (item >= index->m_Count)
        return false;
    uint32_t start = ReadOffset(index->m_Data + index->m_Offsets + item * index->m_OffsetSize, index->m_OffsetSize);
    uint32_t end = ReadOffset(index->m_Data + index->m_Offsets + (item + 1) * index->m_OffsetSize, index->m_OffsetSize);
    if (start == 0 || end < start || end - 1 > index->m_Size - index->m_Objects)
        return false;
    buffer->m_Data = index->m_Data + index->m_Objects + start - 1;
    buffer->m_Size = end - start;
    buffer->m_Position = 0;
    return true;
}

static bool ReadDictNumber(FontTrueTypeBuffer* buffer, float* value)
{
    uint8_t first;
    if (!BufferReadU8(buffer, &first))
        return false;
    if (first >= CFF_NUMBER_FIRST_COMPACT && first <= CFF_NUMBER_LAST_COMPACT)
    {
        *value = first - CFF_NUMBER_COMPACT_ZERO;
        return true;
    }
    uint8_t second;
    if (first >= CFF_NUMBER_FIRST_POSITIVE && first <= CFF_NUMBER_LAST_POSITIVE && BufferReadU8(buffer, &second))
    {
        *value = (first - CFF_NUMBER_FIRST_POSITIVE) * CFF_NUMBER_BYTE_RADIX + second + CFF_NUMBER_TWO_BYTE_OFFSET;
        return true;
    }
    if (first >= CFF_NUMBER_FIRST_NEGATIVE && first <= CFF_NUMBER_LAST_NEGATIVE && BufferReadU8(buffer, &second))
    {
        *value = -(first - CFF_NUMBER_FIRST_NEGATIVE) * CFF_NUMBER_BYTE_RADIX - second - CFF_NUMBER_TWO_BYTE_OFFSET;
        return true;
    }
    if (first == CFF_NUMBER_SHORT_INT)
    {
        uint16_t number;
        if (!BufferReadU16(buffer, &number))
            return false;
        *value = (int16_t)number;
        return true;
    }
    if (first == CFF_NUMBER_LONG_INT)
    {
        uint32_t number;
        if (!BufferReadU32(buffer, &number))
            return false;
        *value = (int32_t)number;
        return true;
    }
    if (first == CFF_NUMBER_REAL)
    {
        while (BufferReadU8(buffer, &second))
            if ((second & CFF_REAL_END_NIBBLE) == CFF_REAL_END_NIBBLE || (second >> 4) == CFF_REAL_END_NIBBLE)
                break;
        *value = 0.0f;
        return true;
    }
    return false;
}

static bool DictGet(const uint8_t* data, uint32_t data_size, uint16_t key, float* values, uint32_t value_count)
{
    // DICT operands precede their operator. Escaped operators use 0x100 plus
    // the second byte so callers can address them with one key.
    FontTrueTypeBuffer buffer = { data, data_size, 0 };
    float              operands[CFF_MAX_DICT_OPERANDS];
    uint32_t           operand_count = 0;
    while (buffer.m_Position < buffer.m_Size)
    {
        uint8_t byte = buffer.m_Data[buffer.m_Position];
        if (byte >= CFF_NUMBER_SHORT_INT)
        {
            if (operand_count == CFF_MAX_DICT_OPERANDS || !ReadDictNumber(&buffer, &operands[operand_count++]))
                return false;
            continue;
        }
        ++buffer.m_Position;
        uint16_t operation = byte;
        if (byte == CFF_DICT_ESCAPE)
        {
            uint8_t escaped;
            if (!BufferReadU8(&buffer, &escaped))
                return false;
            operation = CFF_DICT_ESCAPED_OPERATOR_BASE | escaped;
        }
        if (operation == key)
        {
            uint32_t count = operand_count < value_count ? operand_count : value_count;
            memcpy(values, operands, sizeof(float) * count);
            return count == value_count;
        }
        operand_count = 0;
    }
    return false;
}

static bool GetFD(const FontTrueType* font, uint32_t glyph_index, uint32_t* fd)
{
    // FDSelect chooses the Private DICT for CID-keyed CFF fonts. Read the
    // supported formats directly instead of expanding a per-glyph mapping.
    const uint8_t* data = font->m_FDSelect;
    uint32_t       size = font->m_FDSelectSize;
    if (size == 0)
    {
        *fd = 0;
        return true;
    }
    uint8_t format = data[0];
    if (format == CFF_FDSELECT_FORMAT_ARRAY)
    {
        if (glyph_index >= size - 1)
            return false;
        *fd = data[glyph_index + 1];
        return true;
    }
    if (format == CFF_FDSELECT_FORMAT_RANGE_16)
    {
        if (size < 5)
            return false;
        uint16_t range_count = ReadU16(data + 1);
        uint32_t position = 3;
        uint32_t first = ReadU16(data + position);
        position += 2;
        for (uint16_t i = 0; i < range_count; ++i)
        {
            if (size - position < 3)
                return false;
            uint32_t current_fd = data[position++];
            uint32_t next = ReadU16(data + position);
            position += 2;
            if (glyph_index >= first && glyph_index < next)
            {
                *fd = current_fd;
                return true;
            }
            first = next;
        }
        return false;
    }
    if (format == CFF_FDSELECT_FORMAT_RANGE_32)
    {
        if (size < 9)
            return false;
        uint32_t range_count = ReadU32(data + 1);
        uint32_t position = 5;
        uint32_t first = ReadU32(data + position);
        position += 4;
        for (uint32_t i = 0; i < range_count; ++i)
        {
            if (size - position < 6)
                return false;
            uint32_t current_fd = ReadU16(data + position);
            position += 2;
            uint32_t next = ReadU32(data + position);
            position += 4;
            if (glyph_index >= first && glyph_index < next)
            {
                *fd = current_fd;
                return true;
            }
            first = next;
        }
    }
    return false;
}

static bool ParsePrivate(FontTrueType* font, uint32_t index, FontTrueTypeBuffer font_dict)
{
    // Resolve the Private DICT, local Subrs INDEX, and default CFF2 vsindex once
    // when the face is created.
    FontTrueTypePrivate* result = &font->m_Private[index];
    memset(result, 0, sizeof(*result));
    float private_info[2];
    if (!DictGet(font_dict.m_Data, font_dict.m_Size, CFF_DICT_PRIVATE, private_info, 2))
        return font->m_OutlineType == FONT_OUTLINE_TYPE_CFF1;
    uint32_t private_size = (uint32_t)private_info[0];
    uint32_t private_offset = (uint32_t)private_info[1];
    if (private_offset > font->m_DataSize || private_size > font->m_DataSize - private_offset)
        return false;

    float subrs;
    if (DictGet(font->m_Data + private_offset, private_size, CFF_DICT_SUBRS, &subrs, 1))
    {
        if (subrs < 0 || (uint32_t)subrs > font->m_DataSize - private_offset)
            return false;
        uint32_t end;
        uint32_t count_size = font->m_OutlineType == FONT_OUTLINE_TYPE_CFF1 ? CFF1_INDEX_COUNT_SIZE : CFF2_INDEX_COUNT_SIZE;
        if (!ParseIndex(font->m_Data, font->m_DataSize, private_offset + (uint32_t)subrs, count_size, &result->m_Subrs, &end))
            return false;
    }
    float vsindex;
    if (DictGet(font->m_Data + private_offset, private_size, CFF_DICT_VSINDEX, &vsindex, 1))
        result->m_VSIndex = (uint32_t)vsindex;
    return true;
}

static FontTrueType* NewFont()
{
    FontTrueType* font = new FontTrueType;
    font->m_FontData = 0;
    font->m_Data = 0;
    font->m_DataSize = 0;
    memset(&font->m_GlobalSubrs, 0, sizeof(font->m_GlobalSubrs));
    memset(&font->m_CharStrings, 0, sizeof(font->m_CharStrings));
    memset(&font->m_FontDicts, 0, sizeof(font->m_FontDicts));
    font->m_Charset = 0;
    font->m_CharsetSize = 0;
    font->m_CharsetID = 0;
    font->m_FDSelect = 0;
    font->m_FDSelectSize = 0;
    font->m_VariationStore = 0;
    font->m_VariationStoreSize = 0;
    font->m_CmapOffset = 0;
    font->m_CmapSize = 0;
    font->m_HmtxOffset = 0;
    font->m_LocaOffset = 0;
    font->m_LocaSize = 0;
    font->m_GlyfOffset = 0;
    font->m_GlyfSize = 0;
    font->m_NumGlyphs = 0;
    font->m_NumHMetrics = 0;
    font->m_UnitsPerEm = 0;
    font->m_Ascent = 0;
    font->m_Descent = 0;
    font->m_LineGap = 0;
    font->m_IndexToLocFormat = 0;
    font->m_OutlineType = FONT_OUTLINE_TYPE_GLYF;
    return font;
}

static FontTrueType* InitCFF2Font(const void* source, uint32_t data_size)
{
    // CFF2 stores its Top DICT directly after the header and uses four-byte
    // INDEX counts.
    const uint8_t* data = (const uint8_t*)source;
    if (data_size < 5 || data[0] != CFF_VERSION_2)
        return 0;
    uint32_t header_size = data[2];
    uint32_t top_dict_size = ReadU16(data + 3);
    if (header_size > data_size || top_dict_size > data_size - header_size)
        return 0;

    float char_strings_offset;
    float fd_array_offset;
    if (!DictGet(data + header_size, top_dict_size, CFF_DICT_CHAR_STRINGS, &char_strings_offset, 1) ||
        !DictGet(data + header_size, top_dict_size, CFF_DICT_FD_ARRAY, &fd_array_offset, 1))
        return 0;

    FontTrueType* font = NewFont();
    font->m_Data = data;
    font->m_DataSize = data_size;
    font->m_OutlineType = FONT_OUTLINE_TYPE_CFF2;

    uint32_t end;
    if (!ParseIndex(data, data_size, header_size + top_dict_size, CFF2_INDEX_COUNT_SIZE, &font->m_GlobalSubrs, &end) ||
        !ParseIndex(data, data_size, (uint32_t)char_strings_offset, CFF2_INDEX_COUNT_SIZE, &font->m_CharStrings, &end) ||
        !ParseIndex(data, data_size, (uint32_t)fd_array_offset, CFF2_INDEX_COUNT_SIZE, &font->m_FontDicts, &end))
        goto error;

    font->m_Private.SetCapacity(font->m_FontDicts.m_Count);
    font->m_Private.SetSize(font->m_FontDicts.m_Count);
    for (uint32_t i = 0; i < font->m_FontDicts.m_Count; ++i)
    {
        FontTrueTypeBuffer font_dict;
        if (!IndexGet(&font->m_FontDicts, i, &font_dict) || !ParsePrivate(font, i, font_dict))
            goto error;
    }

    float fd_select_offset;
    if (DictGet(data + header_size, top_dict_size, CFF_DICT_FD_SELECT, &fd_select_offset, 1))
    {
        uint32_t offset = (uint32_t)fd_select_offset;
        if (offset >= data_size)
            goto error;
        font->m_FDSelect = data + offset;
        font->m_FDSelectSize = data_size - offset;
    }

    float variation_store_offset;
    if (DictGet(data + header_size, top_dict_size, CFF_DICT_VSTORE, &variation_store_offset, 1))
    {
        uint32_t offset = (uint32_t)variation_store_offset;
        if (offset > data_size || data_size - offset < 2)
            goto error;
        uint32_t size = ReadU16(data + offset);
        if (size > data_size - offset - 2)
            goto error;
        font->m_VariationStore = data + offset + 2;
        font->m_VariationStoreSize = size;
    }
    return font;

error:
    delete font;
    return 0;
}

static FontTrueType* InitCFF1Font(const void* source, uint32_t data_size)
{
    // CFF1 begins with Name, Top DICT, String, and Global Subrs INDEXes. A
    // non-CID font has one Private DICT; CID fonts provide FDArray/FDSelect.
    const uint8_t* data = (const uint8_t*)source;
    if (data_size < 4 || data[0] != CFF_VERSION_1 || data[2] < 4 || data[2] > data_size)
        return 0;

    FontTrueType* font = NewFont();
    font->m_Data = data;
    font->m_DataSize = data_size;
    font->m_OutlineType = FONT_OUTLINE_TYPE_CFF1;

    FontTrueTypeIndex  name_index;
    FontTrueTypeIndex  top_index;
    FontTrueTypeIndex  string_index;
    FontTrueTypeBuffer top_dict;
    uint32_t           end;
    if (!ParseIndex(data, data_size, data[2], CFF1_INDEX_COUNT_SIZE, &name_index, &end) || name_index.m_Count == 0 ||
        !ParseIndex(data, data_size, end, CFF1_INDEX_COUNT_SIZE, &top_index, &end) || top_index.m_Count == 0 ||
        !ParseIndex(data, data_size, end, CFF1_INDEX_COUNT_SIZE, &string_index, &end) ||
        !ParseIndex(data, data_size, end, CFF1_INDEX_COUNT_SIZE, &font->m_GlobalSubrs, &end) ||
        !IndexGet(&top_index, 0, &top_dict))
        goto error;

    float char_strings_offset;
    if (!DictGet(top_dict.m_Data, top_dict.m_Size, CFF_DICT_CHAR_STRINGS, &char_strings_offset, 1) ||
        !ParseIndex(data, data_size, (uint32_t)char_strings_offset, CFF1_INDEX_COUNT_SIZE, &font->m_CharStrings, &end))
        goto error;

    float charset_offset;
    if (DictGet(top_dict.m_Data, top_dict.m_Size, CFF_DICT_CHARSET, &charset_offset, 1))
    {
        if (charset_offset < 0)
            goto error;
        uint32_t offset = (uint32_t)charset_offset;
        if (offset <= CFF_CHARSET_EXPERT_SUBSET)
        {
            font->m_CharsetID = offset;
        }
        else
        {
            if (offset >= data_size)
                goto error;
            font->m_Charset = data + offset;
            font->m_CharsetSize = data_size - offset;
            font->m_CharsetID = CFF_CHARSET_CUSTOM;
        }
    }

    float fd_array_offset;
    if (DictGet(top_dict.m_Data, top_dict.m_Size, CFF_DICT_FD_ARRAY, &fd_array_offset, 1))
    {
        if (!ParseIndex(data, data_size, (uint32_t)fd_array_offset, CFF1_INDEX_COUNT_SIZE, &font->m_FontDicts, &end))
            goto error;
        font->m_Private.SetCapacity(font->m_FontDicts.m_Count);
        font->m_Private.SetSize(font->m_FontDicts.m_Count);
        for (uint32_t i = 0; i < font->m_FontDicts.m_Count; ++i)
        {
            FontTrueTypeBuffer font_dict;
            if (!IndexGet(&font->m_FontDicts, i, &font_dict) || !ParsePrivate(font, i, font_dict))
                goto error;
        }
        float fd_select_offset;
        if (!DictGet(top_dict.m_Data, top_dict.m_Size, CFF_DICT_FD_SELECT, &fd_select_offset, 1) || fd_select_offset < 0 || (uint32_t)fd_select_offset >= data_size)
            goto error;
        font->m_FDSelect = data + (uint32_t)fd_select_offset;
        font->m_FDSelectSize = data_size - (uint32_t)fd_select_offset;
    }
    else
    {
        font->m_Private.SetCapacity(1);
        font->m_Private.SetSize(1);
        if (!ParsePrivate(font, 0, top_dict))
            goto error;
    }
    return font;

error:
    delete font;
    return 0;
}

FontTrueType* FontTrueTypeCreate(const void* source, uint32_t data_size, uint32_t face_index)
{
    // Parse tables shared by every outline format, then initialize the one
    // outline backend present in the selected face.
    const uint8_t* data = (const uint8_t*)source;
    uint32_t       font_offset;
    uint32_t       cff1_offset, cff1_size;
    uint32_t       cff2_offset, cff2_size;
    uint32_t       cmap_offset, cmap_size;
    uint32_t       head_offset, head_size;
    uint32_t       hhea_offset, hhea_size;
    uint32_t       hmtx_offset, hmtx_size;
    uint32_t       maxp_offset, maxp_size;
    uint32_t       loca_offset, loca_size;
    uint32_t       glyf_offset, glyf_size;
    if (!GetFaceOffset(data, data_size, face_index, &font_offset))
        return 0;
    if (!FindTable(data, data_size, font_offset, FOURCC('c', 'm', 'a', 'p'), &cmap_offset, &cmap_size) ||
        !FindTable(data, data_size, font_offset, FOURCC('h', 'e', 'a', 'd'), &head_offset, &head_size) || head_size < HEAD_REQUIRED_SIZE ||
        !FindTable(data, data_size, font_offset, FOURCC('h', 'h', 'e', 'a'), &hhea_offset, &hhea_size) || hhea_size < HHEA_REQUIRED_SIZE ||
        !FindTable(data, data_size, font_offset, FOURCC('h', 'm', 't', 'x'), &hmtx_offset, &hmtx_size) ||
        !FindTable(data, data_size, font_offset, FOURCC('m', 'a', 'x', 'p'), &maxp_offset, &maxp_size) || maxp_size < MAXP_REQUIRED_SIZE)
        return 0;

    FontTrueType* font = 0;
    if (FindTable(data, data_size, font_offset, FOURCC('C', 'F', 'F', '2'), &cff2_offset, &cff2_size))
    {
        font = InitCFF2Font(data + cff2_offset, cff2_size);
    }
    else if (FindTable(data, data_size, font_offset, FOURCC('C', 'F', 'F', ' '), &cff1_offset, &cff1_size))
    {
        font = InitCFF1Font(data + cff1_offset, cff1_size);
    }
    else if (FindTable(data, data_size, font_offset, FOURCC('l', 'o', 'c', 'a'), &loca_offset, &loca_size) &&
             FindTable(data, data_size, font_offset, FOURCC('g', 'l', 'y', 'f'), &glyf_offset, &glyf_size))
    {
        font = NewFont();
        font->m_LocaOffset = loca_offset;
        font->m_LocaSize = loca_size;
        font->m_GlyfOffset = glyf_offset;
        font->m_GlyfSize = glyf_size;
    }
    if (!font)
        return 0;

    font->m_FontData = data;
    font->m_UnitsPerEm = ReadU16(data + head_offset + HEAD_UNITS_PER_EM_OFFSET);
    font->m_Ascent = ReadS16(data + hhea_offset + HHEA_ASCENT_OFFSET);
    font->m_Descent = ReadS16(data + hhea_offset + HHEA_DESCENT_OFFSET);
    font->m_LineGap = ReadS16(data + hhea_offset + HHEA_LINE_GAP_OFFSET);
    font->m_NumHMetrics = ReadU16(data + hhea_offset + HHEA_METRIC_COUNT_OFFSET);
    font->m_NumGlyphs = ReadU16(data + maxp_offset + MAXP_GLYPH_COUNT_OFFSET);
    font->m_HmtxOffset = hmtx_offset;
    font->m_IndexToLocFormat = ReadS16(data + head_offset + HEAD_LOCA_FORMAT_OFFSET);
    uint32_t loca_entry_size = font->m_IndexToLocFormat == LOCA_FORMAT_SHORT ? 2 : 4;
    if (font->m_UnitsPerEm == 0 || font->m_NumHMetrics == 0 || font->m_NumHMetrics > font->m_NumGlyphs ||
        font->m_NumHMetrics * 4 + (font->m_NumGlyphs - font->m_NumHMetrics) * 2 > hmtx_size ||
        (font->m_OutlineType != FONT_OUTLINE_TYPE_GLYF && font->m_CharStrings.m_Count != font->m_NumGlyphs) ||
        (font->m_OutlineType == FONT_OUTLINE_TYPE_GLYF &&
         (font->m_IndexToLocFormat < LOCA_FORMAT_SHORT || font->m_IndexToLocFormat > LOCA_FORMAT_LONG || loca_entry_size * (font->m_NumGlyphs + 1) > font->m_LocaSize)) ||
        !SelectCmap(font, cmap_offset, cmap_size))
    {
        delete font;
        return 0;
    }
    return font;
}

void FontTrueTypeDestroy(FontTrueType* font)
{
    delete font;
}

struct FontTrueTypeOutlineBuilder
{
    // Type 2 coordinates are relative. The builder accumulates them and emits
    // absolute FontOutline commands, or measures them without allocation when
    // m_Bounds is set.
    dmArray<FontOutlineCommand> m_Commands;
    FontOutlineBounds*          m_Bounds;
    float                       m_X;
    float                       m_Y;
    uint32_t                    m_PathCommandCount;
    bool                        m_ContourOpen;
};

static FontOutlineCommand* PushCommand(FontTrueTypeOutlineBuilder* builder, FontOutlineCommandType type)
{
    if (builder->m_Commands.Full())
        builder->m_Commands.OffsetCapacity(OUTLINE_ARRAY_GROWTH);
    FontOutlineCommand command;
    memset(&command, 0, sizeof(command));
    command.m_Type = type;
    builder->m_Commands.Push(command);
    return &builder->m_Commands.Back();
}

static void CloseContour(FontTrueTypeOutlineBuilder* builder)
{
    if (builder->m_ContourOpen && !builder->m_Bounds)
        PushCommand(builder, FONT_OUTLINE_CLOSE);
    builder->m_ContourOpen = false;
}

static void MoveTo(FontTrueTypeOutlineBuilder* builder, float dx, float dy)
{
    CloseContour(builder);
    builder->m_X += dx;
    builder->m_Y += dy;
    FontOutlinePoint to = { builder->m_X, builder->m_Y };
    if (builder->m_Bounds)
        FontOutlineBoundsMoveTo(builder->m_Bounds, to);
    else
        PushCommand(builder, FONT_OUTLINE_MOVE_TO)->m_Points[0] = to;
    ++builder->m_PathCommandCount;
    builder->m_ContourOpen = true;
}

static void LineTo(FontTrueTypeOutlineBuilder* builder, float dx, float dy)
{
    builder->m_X += dx;
    builder->m_Y += dy;
    FontOutlinePoint to = { builder->m_X, builder->m_Y };
    if (builder->m_Bounds)
        FontOutlineBoundsLineTo(builder->m_Bounds, to);
    else
        PushCommand(builder, FONT_OUTLINE_LINE_TO)->m_Points[0] = to;
    ++builder->m_PathCommandCount;
}

static void CurveTo(FontTrueTypeOutlineBuilder* builder, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
    FontOutlinePoint control_1 = { builder->m_X + dx1, builder->m_Y + dy1 };
    FontOutlinePoint control_2 = { control_1.m_X + dx2, control_1.m_Y + dy2 };
    builder->m_X = control_2.m_X + dx3;
    builder->m_Y = control_2.m_Y + dy3;
    FontOutlinePoint to = { builder->m_X, builder->m_Y };
    if (builder->m_Bounds)
    {
        FontOutlineBoundsCubicTo(builder->m_Bounds, control_1, control_2, to);
    }
    else
    {
        FontOutlineCommand* command = PushCommand(builder, FONT_OUTLINE_CUBIC_TO);
        command->m_Points[0] = control_1;
        command->m_Points[1] = control_2;
        command->m_Points[2] = to;
    }
    ++builder->m_PathCommandCount;
}

static bool GetSubroutine(const FontTrueTypeIndex* index, int32_t number, FontTrueTypeBuffer* buffer)
{
    // Type 2 CharStrings encode subroutine operands relative to a bias whose
    // value depends on the number of entries in the Subrs INDEX. These
    // thresholds and biases are defined by the Type 2 CharString
    // specification, section 4.7.
    int32_t bias;
    if (index->m_Count < TYPE2_SMALL_SUBROUTINE_LIMIT)
        bias = TYPE2_SMALL_SUBROUTINE_BIAS;
    else if (index->m_Count < TYPE2_MEDIUM_SUBROUTINE_LIMIT)
        bias = TYPE2_MEDIUM_SUBROUTINE_BIAS;
    else
        bias = TYPE2_LARGE_SUBROUTINE_BIAS;
    number += bias;
    return number >= 0 && IndexGet(index, (uint32_t)number, buffer);
}

static float RegionAxisScalar(float coordinate, float start, float peak, float end)
{
    if (peak == 0.0f)
        return 1.0f;
    if (coordinate <= start || coordinate >= end)
        return 0.0f;
    if (coordinate == peak)
        return 1.0f;
    return coordinate < peak ? (coordinate - start) / (peak - start) : (end - coordinate) / (end - peak);
}

static bool GetVariationScalars(const FontTrueType* font, uint32_t vsindex, float* scalars, uint32_t scalar_capacity, uint32_t* scalar_count)
{
    // FontTrueType selects the default variable-font instance, so each axis
    // coordinate is zero when evaluating variation regions.
    if (!font->m_VariationStore)
        return false;
    const uint8_t* store = font->m_VariationStore;
    uint32_t       size = font->m_VariationStoreSize;
    if (size < 8 || ReadU16(store) != 1)
        return false;
    uint32_t region_list_offset = ReadU32(store + 2);
    uint16_t data_count = ReadU16(store + 6);
    if (vsindex >= data_count || size - 8 < (uint32_t)data_count * 4)
        return false;
    uint32_t item_data_offset = ReadU32(store + 8 + vsindex * 4);
    if (item_data_offset > size || size - item_data_offset < 6)
        return false;
    const uint8_t* item_data = store + item_data_offset;
    uint16_t       region_index_count = ReadU16(item_data + 4);
    if (region_index_count > scalar_capacity || size - item_data_offset - 6 < (uint32_t)region_index_count * 2)
        return false;
    if (region_list_offset > size || size - region_list_offset < 4)
        return false;
    const uint8_t* region_list = store + region_list_offset;
    uint16_t       axis_count = ReadU16(region_list);
    uint16_t       region_count = ReadU16(region_list + 2);
    if (size - region_list_offset - 4 < (uint32_t)axis_count * region_count * 6)
        return false;

    *scalar_count = region_index_count;
    for (uint16_t i = 0; i < region_index_count; ++i)
    {
        uint16_t region_index = ReadU16(item_data + 6 + i * 2);
        if (region_index >= region_count)
            return false;
        float          scalar = 1.0f;
        const uint8_t* axes = region_list + 4 + (uint32_t)region_index * axis_count * 6;
        for (uint16_t axis = 0; axis < axis_count; ++axis)
        {
            float start = ReadS16(axes + axis * 6) / F2DOT14_SCALE;
            float peak = ReadS16(axes + axis * 6 + 2) / F2DOT14_SCALE;
            float end = ReadS16(axes + axis * 6 + 4) / F2DOT14_SCALE;
            scalar *= RegionAxisScalar(0.0f, start, peak, end);
        }
        scalars[i] = scalar;
    }
    return true;
}

static bool Blend(const FontTrueType* font, uint32_t vsindex, float* stack, uint32_t* stack_size)
{
    if (*stack_size == 0)
        return false;
    uint32_t value_count = (uint32_t)stack[*stack_size - 1];
    // A Type 2 CharString has at most 513 stack operands, which also bounds
    // the number of variation regions a blend operation can reference. Keep
    // this short-lived scratch data on the stack to avoid one allocation per
    // blend operator.
    float    scalars[MAX_TYPE2_VARIATION_SCALARS];
    uint32_t scalar_count = 0;
    if (!GetVariationScalars(font, vsindex, scalars, 512, &scalar_count))
        return false;
    uint64_t required = (uint64_t)value_count * (scalar_count + 1) + 1;
    if (required > *stack_size)
        return false;
    uint32_t start = *stack_size - (uint32_t)required;
    for (uint32_t value = 0; value < value_count; ++value)
    {
        float result = stack[start + value];
        for (uint32_t region = 0; region < scalar_count; ++region)
            result += stack[start + value_count + value * scalar_count + region] * scalars[region];
        stack[start + value] = result;
    }
    *stack_size = start + value_count;
    return true;
}

// Adobe StandardEncoding code to CFF standard string identifier. The table is
// used only by the deprecated CFF1 endchar composition form.
static const uint8_t STANDARD_ENCODING_SIDS[CFF_STANDARD_ENCODING_CODE_COUNT] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
     33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
     65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
     81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
      0, 111, 112, 113, 114,   0, 115, 116, 117, 118, 119, 120, 121, 122,   0, 123,
      0, 124, 125, 126, 127, 128, 129, 130, 131,   0, 132, 133,   0, 134, 135, 136,
    137,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0, 138,   0, 139,   0,   0,   0,   0, 140, 141, 142, 143,   0,   0,   0,   0,
      0, 144,   0,   0,   0, 145,   0,   0, 146, 147, 148, 149,   0,   0,   0,   0,
};

static const uint16_t EXPERT_CHARSET_SIDS[] = {
      0,   1, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,  13,  14,  15,  99,
    239, 240, 241, 242, 243, 244, 245, 246, 247, 248,  27,  28, 249, 250, 251, 252,
    253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 109, 110,
    267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282,
    283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298,
    299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313, 314,
    315, 316, 317, 318, 158, 155, 163, 319, 320, 321, 322, 323, 324, 325, 326, 150,
    164, 169, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340,
    341, 342, 343, 344, 345, 346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356,
    357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372,
    373, 374, 375, 376, 377, 378,
};

static const uint16_t EXPERT_SUBSET_CHARSET_SIDS[] = {
      0,   1, 231, 232, 235, 236, 237, 238,  13,  14,  15,  99, 239, 240, 241, 242,
    243, 244, 245, 246, 247, 248,  27,  28, 249, 250, 251, 253, 254, 255, 256, 257,
    258, 259, 260, 261, 262, 263, 264, 265, 266, 109, 110, 267, 268, 269, 270, 272,
    300, 301, 302, 305, 314, 315, 158, 155, 163, 320, 321, 322, 323, 324, 325, 326,
    150, 164, 169, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339,
    340, 341, 342, 343, 344, 345, 346,
};

static uint32_t GetCFFGlyphForSID(const FontTrueType* font, uint32_t sid)
{
    if (font->m_CharsetID == CFF_CHARSET_ISO_ADOBE)
        return sid < font->m_NumGlyphs && sid <= CFF_STANDARD_CHARSET_LAST_SID ? sid : 0;
    if (font->m_CharsetID == CFF_CHARSET_EXPERT || font->m_CharsetID == CFF_CHARSET_EXPERT_SUBSET)
    {
        const uint16_t* charset;
        uint32_t        charset_size;
        if (font->m_CharsetID == CFF_CHARSET_EXPERT)
        {
            charset = EXPERT_CHARSET_SIDS;
            charset_size = sizeof(EXPERT_CHARSET_SIDS) / sizeof(EXPERT_CHARSET_SIDS[0]);
        }
        else
        {
            charset = EXPERT_SUBSET_CHARSET_SIDS;
            charset_size = sizeof(EXPERT_SUBSET_CHARSET_SIDS) / sizeof(EXPERT_SUBSET_CHARSET_SIDS[0]);
        }
        uint32_t glyph_count = font->m_NumGlyphs < charset_size ? font->m_NumGlyphs : charset_size;
        for (uint32_t glyph = 1; glyph < glyph_count; ++glyph)
            if (charset[glyph] == sid)
                return glyph;
        return 0;
    }
    if (font->m_CharsetID != CFF_CHARSET_CUSTOM || !font->m_Charset || font->m_CharsetSize == 0)
        return 0;

    const uint8_t* data = font->m_Charset;
    uint32_t       size = font->m_CharsetSize;
    uint32_t       position = 1;
    uint32_t       glyph = 1;
    uint8_t        format = data[0];
    if (format == CFF_CHARSET_FORMAT_ARRAY)
    {
        if (font->m_NumGlyphs - 1 > (size - position) / 2)
            return 0;
        for (; glyph < font->m_NumGlyphs; ++glyph)
            if (ReadU16(data + position + (glyph - 1) * 2) == sid)
                return glyph;
        return 0;
    }
    if (format != CFF_CHARSET_FORMAT_RANGE_8 && format != CFF_CHARSET_FORMAT_RANGE_16)
        return 0;
    while (glyph < font->m_NumGlyphs)
    {
        uint32_t range_size = format == CFF_CHARSET_FORMAT_RANGE_8 ? 3 : 4;
        if (position > size || range_size > size - position)
            return 0;
        uint32_t first = ReadU16(data + position);
        uint32_t count = format == CFF_CHARSET_FORMAT_RANGE_8 ? data[position + 2] : ReadU16(data + position + 2);
        position += range_size;
        if (count >= font->m_NumGlyphs - glyph || first + count > MAX_BMP_CODEPOINT)
            return 0;
        if (sid >= first && sid <= first + count)
            return glyph + sid - first;
        glyph += count + 1;
    }
    return 0;
}

static uint32_t GetCFFGlyphForStandardCode(const FontTrueType* font, float value)
{
    if (value < 0.0f || value > 255.0f || value != floorf(value))
        return 0;
    uint32_t sid = STANDARD_ENCODING_SIDS[(uint32_t)value];
    return sid ? GetCFFGlyphForSID(font, sid) : 0;
}

static uint32_t GetOutlinePointCount(FontOutlineCommandType type)
{
    switch (type)
    {
        case FONT_OUTLINE_MOVE_TO:      return 1;
        case FONT_OUTLINE_LINE_TO:      return 1;
        case FONT_OUTLINE_QUADRATIC_TO: return 2;
        case FONT_OUTLINE_CUBIC_TO:     return 3;
        case FONT_OUTLINE_CLOSE:        return 0;
    }
    return 0;
}

static void AppendOutline(FontTrueTypeOutlineBuilder* builder, const FontTrueTypeOutlineBuilder& source, float dx, float dy)
{
    if (builder->m_Bounds)
    {
        FontOutlineBoundsMerge(builder->m_Bounds, source.m_Bounds, dx, dy);
        builder->m_PathCommandCount += source.m_PathCommandCount;
        return;
    }

    for (uint32_t i = 0; i < source.m_Commands.Size(); ++i)
    {
        FontOutlineCommand command = source.m_Commands[i];
        uint32_t point_count = GetOutlinePointCount(command.m_Type);
        for (uint32_t point = 0; point < point_count; ++point)
        {
            command.m_Points[point].m_X += dx;
            command.m_Points[point].m_Y += dy;
        }
        if (builder->m_Commands.Full())
            builder->m_Commands.OffsetCapacity(OUTLINE_ARRAY_GROWTH);
        builder->m_Commands.Push(command);
    }
    builder->m_PathCommandCount += source.m_PathCommandCount;
}

static bool ReadCharStringNumber(FontTrueTypeBuffer* buffer, uint8_t first, float* value)
{
    // Type 2 encodes compact integers directly in the first byte. Values
    // 32..246 represent -107..107 after subtracting the specified bias.
    if (first >= CFF_NUMBER_FIRST_COMPACT && first <= CFF_NUMBER_LAST_COMPACT)
    {
        *value = first - CFF_NUMBER_COMPACT_ZERO;
        return true;
    }
    uint8_t second;
    // 247..250 encode progressively larger positive two-byte integers.
    if (first >= CFF_NUMBER_FIRST_POSITIVE && first <= CFF_NUMBER_LAST_POSITIVE && BufferReadU8(buffer, &second))
    {
        *value = (first - CFF_NUMBER_FIRST_POSITIVE) * CFF_NUMBER_BYTE_RADIX + second + CFF_NUMBER_TWO_BYTE_OFFSET;
        return true;
    }
    // 251..254 use the same two-byte form for negative integers.
    if (first >= CFF_NUMBER_FIRST_NEGATIVE && first <= CFF_NUMBER_LAST_NEGATIVE && BufferReadU8(buffer, &second))
    {
        *value = -(first - CFF_NUMBER_FIRST_NEGATIVE) * CFF_NUMBER_BYTE_RADIX - second - CFF_NUMBER_TWO_BYTE_OFFSET;
        return true;
    }
    // Operator byte 28 is the shortint marker followed by a signed int16.
    if (first == CFF_NUMBER_SHORT_INT)
    {
        uint16_t number;
        if (!BufferReadU16(buffer, &number))
            return false;
        *value = (int16_t)number;
        return true;
    }
    // Byte 255 is followed by a signed 16.16 fixed-point number.
    if (first == CFF_NUMBER_FIXED_16_16)
    {
        uint32_t number;
        if (!BufferReadU32(buffer, &number))
            return false;
        *value = (float)(int32_t)number / 65536.0f;
        return true;
    }
    return false;
}

static bool InterpretCharString(FontTrueType* font, uint32_t glyph_index, FontTrueTypeOutlineBuilder* builder, uint32_t depth)
{
    // Execute Type 2 CharStrings iteratively. Operands use one shared stack;
    // local and global subroutine calls retain bounded return buffers. CFF1
    // endchar composition can recurse into two other glyphs, so cap that
    // separate recursion as protection against malformed cyclic data.
    if (depth == MAX_CFF_ENDCHAR_RECURSION)
        return false;
    FontTrueTypeBuffer buffer;
    if (!IndexGet(&font->m_CharStrings, glyph_index, &buffer))
        return false;
    uint32_t fd;
    if (!GetFD(font, glyph_index, &fd) || fd >= font->m_Private.Size())
        return false;
    FontTrueTypePrivate* private_data = &font->m_Private[fd];

    // The Type 2 specification limits subroutine nesting to ten and the CFF2
    // operand stack to 513 values.
    FontTrueTypeBuffer   return_stack[MAX_TYPE2_SUBROUTINE_DEPTH];
    uint32_t             return_count = 0;
    float                stack[MAX_TYPE2_OPERANDS];
    uint32_t             stack_size = 0;
    uint32_t             mask_bits = 0;
    uint32_t             vsindex = private_data->m_VSIndex;
    bool                 width_parsed = font->m_OutlineType != FONT_OUTLINE_TYPE_CFF1;
    while (true)
    {
        if (buffer.m_Position == buffer.m_Size)
        {
            if (return_count == 0)
                break;
            buffer = return_stack[--return_count];
            continue;
        }
        uint8_t operation;
        if (!BufferReadU8(&buffer, &operation))
            return false;
        // Bytes 0..31 are operators, except shortint (28). Bytes 32..254 are
        // compact integer operands and byte 255 is a 16.16 operand.
        if (operation == CFF_NUMBER_SHORT_INT || operation == CFF_NUMBER_FIXED_16_16 || operation >= CFF_NUMBER_FIRST_COMPACT)
        {
            if (stack_size == MAX_TYPE2_OPERANDS || !ReadCharStringNumber(&buffer, operation, &stack[stack_size++]))
                return false;
            continue;
        }

        if (!width_parsed)
        {
            // CFF1 encodes an optional width as the extra first operand of the
            // first stem, mask, moveto, or endchar operator. Remove it before
            // interpreting the remaining operands. CFF2 does not encode glyph
            // widths in CharStrings.
            uint32_t expected = 0;
            if (operation == TYPE2_RMOVETO)
                expected = 2;
            else if (operation == TYPE2_VMOVETO || operation == TYPE2_HMOVETO)
                expected = 1;
            else if (operation == TYPE2_ENDCHAR)
                expected = 0;
            // hstem, vstem, hstemhm, hintmask, cntrmask, and vstemhm all
            // consume pairs of stem operands.
            if (operation == TYPE2_HSTEM || operation == TYPE2_VSTEM || operation == TYPE2_HSTEMHM ||
                operation == TYPE2_HINTMASK || operation == TYPE2_CNTRMASK || operation == TYPE2_VSTEMHM)
            {
                if (stack_size & 1)
                {
                    memmove(stack, stack + 1, sizeof(float) * --stack_size);
                }
                width_parsed = true;
            }
            else if (operation == TYPE2_VMOVETO || operation == TYPE2_RMOVETO || operation == TYPE2_HMOVETO || operation == TYPE2_ENDCHAR)
            {
                if (stack_size == expected + 1 || (operation == TYPE2_ENDCHAR && stack_size == 5))
                {
                    memmove(stack, stack + 1, sizeof(float) * --stack_size);
                }
                width_parsed = true;
            }
        }

        uint32_t i = 0;
        bool     clear_stack = true;
        switch (operation)
        {
            case TYPE2_HSTEM:
            case TYPE2_VSTEM:
            case TYPE2_HSTEMHM:
            case TYPE2_VSTEMHM:
                mask_bits += stack_size / 2;
                break;
            case TYPE2_HINTMASK:
            case TYPE2_CNTRMASK:
                mask_bits += stack_size / 2;
                if ((mask_bits + 7) / 8 > buffer.m_Size - buffer.m_Position)
                    return false;
                buffer.m_Position += (mask_bits + 7) / 8;
                break;
            case TYPE2_RMOVETO:
                if (stack_size < 2)
                    return false;
                MoveTo(builder, stack[stack_size - 2], stack[stack_size - 1]);
                break;
            case TYPE2_VMOVETO:
                if (stack_size < 1)
                    return false;
                MoveTo(builder, 0.0f, stack[stack_size - 1]);
                break;
            case TYPE2_HMOVETO:
                if (stack_size < 1)
                    return false;
                MoveTo(builder, stack[stack_size - 1], 0.0f);
                break;
            case TYPE2_RLINETO:
                if (stack_size < 2)
                    return false;
                for (; i + 1 < stack_size; i += 2)
                    LineTo(builder, stack[i], stack[i + 1]);
                break;
            case TYPE2_HLINETO:
            case TYPE2_VLINETO:
                if (stack_size < 1)
                    return false;
                for (; i < stack_size; ++i)
                    if (((i + (operation == TYPE2_VLINETO)) & 1) == 0)
                        LineTo(builder, stack[i], 0.0f);
                    else
                        LineTo(builder, 0.0f, stack[i]);
                break;
            case TYPE2_RRCURVETO:
                if (stack_size < 6)
                    return false;
                for (; i + 5 < stack_size; i += 6)
                    CurveTo(builder, stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5]);
                break;
            case TYPE2_RCURVELINE:
                if (stack_size < 8)
                    return false;
                for (; i + 5 < stack_size - 2; i += 6)
                    CurveTo(builder, stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5]);
                LineTo(builder, stack[i], stack[i + 1]);
                break;
            case TYPE2_RLINECURVE:
                if (stack_size < 8)
                    return false;
                for (; i + 1 < stack_size - 6; i += 2)
                    LineTo(builder, stack[i], stack[i + 1]);
                CurveTo(builder, stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5]);
                break;
            case TYPE2_VVCURVETO:
            case TYPE2_HHCURVETO:
            {
                if (stack_size < 4)
                    return false;
                float first = 0.0f;
                if (stack_size & 1)
                    first = stack[i++];
                for (; i + 3 < stack_size; i += 4)
                {
                    if (operation == TYPE2_HHCURVETO)
                        CurveTo(builder, stack[i], first, stack[i + 1], stack[i + 2], stack[i + 3], 0.0f);
                    else
                        CurveTo(builder, first, stack[i], stack[i + 1], stack[i + 2], 0.0f, stack[i + 3]);
                    first = 0.0f;
                }
            }
            break;
            case TYPE2_VHCURVETO:
            case TYPE2_HVCURVETO:
                if (stack_size < 4)
                    return false;
                while (i + 3 < stack_size)
                {
                    if (operation == TYPE2_VHCURVETO)
                        CurveTo(builder, 0.0f, stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack_size - i == 5 ? stack[i + 4] : 0.0f);
                    else
                        CurveTo(builder, stack[i], 0.0f, stack[i + 1], stack[i + 2], stack_size - i == 5 ? stack[i + 4] : 0.0f, stack[i + 3]);
                    i += 4;
                    operation = operation == TYPE2_VHCURVETO ? TYPE2_HVCURVETO : TYPE2_VHCURVETO;
                }
                break;
            case TYPE2_CALLSUBR:
            case TYPE2_CALLGSUBR:
            {
                if (stack_size == 0 || return_count == MAX_TYPE2_SUBROUTINE_DEPTH)
                    return false;
                int32_t subroutine = (int32_t)stack[--stack_size];
                return_stack[return_count++] = buffer;
                FontTrueTypeIndex* index = operation == TYPE2_CALLSUBR ? &private_data->m_Subrs : &font->m_GlobalSubrs;
                if (!GetSubroutine(index, subroutine, &buffer))
                    return false;
                clear_stack = false;
            }
            break;
            case TYPE2_RETURN:
                if (return_count == 0)
                    return false;
                buffer = return_stack[--return_count];
                clear_stack = false;
                break;
            case TYPE2_ENDCHAR:
                if (stack_size == 4 && font->m_OutlineType == FONT_OUTLINE_TYPE_CFF1 && builder->m_PathCommandCount == 0)
                {
                    uint32_t base_glyph = GetCFFGlyphForStandardCode(font, stack[2]);
                    uint32_t accent_glyph = GetCFFGlyphForStandardCode(font, stack[3]);
                    if (!base_glyph || !accent_glyph)
                        return false;
                    FontTrueTypeOutlineBuilder base = {};
                    FontTrueTypeOutlineBuilder accent = {};
                    FontOutlineBounds base_bounds = {};
                    FontOutlineBounds accent_bounds = {};
                    if (builder->m_Bounds)
                    {
                        base.m_Bounds = &base_bounds;
                        accent.m_Bounds = &accent_bounds;
                    }
                    if (!InterpretCharString(font, base_glyph, &base, depth + 1) ||
                        !InterpretCharString(font, accent_glyph, &accent, depth + 1))
                        return false;
                    AppendOutline(builder, base, 0.0f, 0.0f);
                    AppendOutline(builder, accent, stack[0], stack[1]);
                }
                else if (stack_size != 0)
                    return false;
                CloseContour(builder);
                return true;
            case TYPE2_VSINDEX:
                if (font->m_OutlineType != FONT_OUTLINE_TYPE_CFF2)
                    return false;
                if (stack_size != 1)
                    return false;
                vsindex = (uint32_t)stack[0];
                break;
            case TYPE2_BLEND:
                if (font->m_OutlineType != FONT_OUTLINE_TYPE_CFF2)
                    return false;
                if (!Blend(font, vsindex, stack, &stack_size))
                    return false;
                clear_stack = false;
                break;
            case TYPE2_ESCAPE:
            {
                uint8_t escaped;
                if (!BufferReadU8(&buffer, &escaped))
                    return false;
                float dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4, dx5, dy5, dx6, dy6;
                switch (escaped)
                {
                    case TYPE2_HFLEX:
                        if (stack_size < 7)
                            return false;
                        CurveTo(builder, stack[0], 0, stack[1], stack[2], stack[3], 0);
                        CurveTo(builder, stack[4], 0, stack[5], -stack[2], stack[6], 0);
                        break;
                    case TYPE2_FLEX:
                        if (stack_size < 13)
                            return false;
                        CurveTo(builder, stack[0], stack[1], stack[2], stack[3], stack[4], stack[5]);
                        CurveTo(builder, stack[6], stack[7], stack[8], stack[9], stack[10], stack[11]);
                        break;
                    case TYPE2_HFLEX1:
                        if (stack_size < 9)
                            return false;
                        CurveTo(builder, stack[0], stack[1], stack[2], stack[3], stack[4], 0);
                        CurveTo(builder, stack[5], 0, stack[6], stack[7], stack[8], -(stack[1] + stack[3] + stack[7]));
                        break;
                    case TYPE2_FLEX1:
                        if (stack_size < 11)
                            return false;
                        dx1 = stack[0];
                        dy1 = stack[1];
                        dx2 = stack[2];
                        dy2 = stack[3];
                        dx3 = stack[4];
                        dy3 = stack[5];
                        dx4 = stack[6];
                        dy4 = stack[7];
                        dx5 = stack[8];
                        dy5 = stack[9];
                        dx6 = dy6 = stack[10];
                        if (fabsf(dx1 + dx2 + dx3 + dx4 + dx5) > fabsf(dy1 + dy2 + dy3 + dy4 + dy5))
                            dy6 = -(dy1 + dy2 + dy3 + dy4 + dy5);
                        else
                            dx6 = -(dx1 + dx2 + dx3 + dx4 + dx5);
                        CurveTo(builder, dx1, dy1, dx2, dy2, dx3, dy3);
                        CurveTo(builder, dx4, dy4, dx5, dy5, dx6, dy6);
                        break;
                    default:
                        return false;
                }
            }
            break;
            default:
                return false;
        }
        if (clear_stack)
            stack_size = 0;
    }
    CloseContour(builder);
    return true;
}

struct FontTrueTypePoint
{
    // Decoded glyf point before implied on-curve points are introduced.
    float m_X;
    float m_Y;
    bool  m_OnCurve;
};

struct FontTrueTypeGlyf
{
    dmArray<FontOutlineCommand> m_Commands;
    dmArray<FontTrueTypePoint>  m_Points;
};

static void PushOutlineCommand(dmArray<FontOutlineCommand>& commands, FontOutlineCommandType type, FontOutlinePoint p0 = {}, FontOutlinePoint p1 = {}, FontOutlinePoint p2 = {})
{
    if (commands.Full())
        commands.OffsetCapacity(OUTLINE_ARRAY_GROWTH);
    FontOutlineCommand command = {};
    command.m_Type = type;
    command.m_Points[0] = p0;
    command.m_Points[1] = p1;
    command.m_Points[2] = p2;
    commands.Push(command);
}

static uint32_t GetGlyphOffset(FontTrueType* font, uint32_t glyph_index)
{
    const uint8_t* loca = font->m_FontData + font->m_LocaOffset;
    if (font->m_IndexToLocFormat == LOCA_FORMAT_SHORT)
        return ReadU16(loca + glyph_index * 2) * 2;
    return ReadU32(loca + glyph_index * 4);
}

static bool GetGlyphData(FontTrueType* font, uint32_t glyph_index, const uint8_t** data, uint32_t* data_size)
{
    if (glyph_index >= font->m_NumGlyphs)
        return false;
    uint32_t begin = GetGlyphOffset(font, glyph_index);
    uint32_t end = GetGlyphOffset(font, glyph_index + 1);
    if (begin > end || end > font->m_GlyfSize)
        return false;
    *data = font->m_FontData + font->m_GlyfOffset + begin;
    *data_size = end - begin;
    return true;
}

static FontOutlinePoint Midpoint(const FontTrueTypePoint& a, const FontTrueTypePoint& b)
{
    FontOutlinePoint result = { (a.m_X + b.m_X) * 0.5f, (a.m_Y + b.m_Y) * 0.5f };
    return result;
}

static FontOutlinePoint ToOutlinePoint(const FontTrueTypePoint& point)
{
    FontOutlinePoint result = { point.m_X, point.m_Y };
    return result;
}

static bool BuildSimpleContours(const dmArray<FontTrueTypePoint>& points, const uint16_t* ends, uint32_t contour_count, dmArray<FontOutlineCommand>& commands)
{
    // Consecutive off-curve points imply an on-curve midpoint. A contour that
    // starts off-curve similarly derives its start from the final/first pair.
    uint32_t first = 0;
    for (uint32_t contour = 0; contour < contour_count; ++contour)
    {
        uint32_t last = ends[contour];
        if (last < first || last >= points.Size())
            return false;
        uint32_t         count = last - first + 1;
        uint32_t         position;
        uint32_t         remaining;
        FontOutlinePoint start;
        if (points[first].m_OnCurve)
        {
            start = ToOutlinePoint(points[first]);
            position = first + 1;
            remaining = count - 1;
        }
        else if (points[last].m_OnCurve)
        {
            start = ToOutlinePoint(points[last]);
            position = first;
            remaining = count - 1;
        }
        else
        {
            start = Midpoint(points[last], points[first]);
            position = first;
            remaining = count;
        }
        PushOutlineCommand(commands, FONT_OUTLINE_MOVE_TO, start);
        while (remaining > 0)
        {
            const FontTrueTypePoint& point = points[position];
            if (point.m_OnCurve)
            {
                PushOutlineCommand(commands, FONT_OUTLINE_LINE_TO, ToOutlinePoint(point));
                position = position == last ? first : position + 1;
                --remaining;
                continue;
            }

            uint32_t                 next_position = position == last ? first : position + 1;
            const FontTrueTypePoint& next = points[next_position];
            if (next.m_OnCurve)
            {
                PushOutlineCommand(commands, FONT_OUTLINE_QUADRATIC_TO, ToOutlinePoint(point), ToOutlinePoint(next));
                position = next_position == last ? first : next_position + 1;
                remaining -= remaining > 1 ? 2 : 1;
            }
            else
            {
                PushOutlineCommand(commands, FONT_OUTLINE_QUADRATIC_TO, ToOutlinePoint(point), Midpoint(point, next));
                position = next_position;
                --remaining;
            }
        }
        PushOutlineCommand(commands, FONT_OUTLINE_CLOSE);
        first = last + 1;
    }
    return true;
}

static bool DecodeSimpleGlyf(const uint8_t* data, uint32_t data_size, uint32_t contour_count, FontTrueTypeGlyf* result)
{
    // Flags are run-length encoded. X and Y follow as independent delta
    // streams whose short/same bits are stored in those flags.
    uint32_t position = GLYF_HEADER_SIZE;
    if (contour_count > (data_size - position) / 2)
        return false;
    dmArray<uint16_t> ends;
    ends.SetCapacity(contour_count);
    ends.SetSize(contour_count);
    for (uint32_t i = 0; i < contour_count; ++i)
        ends[i] = ReadU16(data + position + i * 2);
    position += contour_count * 2;
    if (data_size - position < 2)
        return false;
    uint32_t instruction_size = ReadU16(data + position);
    position += 2;
    if (instruction_size > data_size - position)
        return false;
    position += instruction_size;

    uint32_t         point_count = contour_count == 0 ? 0 : ends.Back() + 1;
    dmArray<uint8_t> flags;
    flags.SetCapacity(point_count);
    while (flags.Size() < point_count)
    {
        if (position == data_size)
            return false;
        uint8_t flag = data[position++];
        flags.Push(flag);
        if (flag & GLYF_FLAG_REPEAT)
        {
            if (position == data_size)
                return false;
            uint32_t repeat = data[position++];
            if (repeat > point_count - flags.Size())
                return false;
            for (uint32_t i = 0; i < repeat; ++i)
                flags.Push(flag);
        }
    }

    result->m_Points.SetCapacity(point_count);
    result->m_Points.SetSize(point_count);
    int32_t coordinate = 0;
    for (uint32_t i = 0; i < point_count; ++i)
    {
        uint8_t flag = flags[i];
        if (flag & GLYF_FLAG_X_SHORT)
        {
            if (position == data_size)
                return false;
            int32_t delta = data[position++];
            coordinate += flag & GLYF_FLAG_X_SAME ? delta : -delta;
        }
        else if (!(flag & GLYF_FLAG_X_SAME))
        {
            if (data_size - position < 2)
                return false;
            coordinate += ReadS16(data + position);
            position += 2;
        }
        result->m_Points[i].m_X = coordinate;
        result->m_Points[i].m_OnCurve = (flag & GLYF_FLAG_ON_CURVE) != 0;
    }
    coordinate = 0;
    for (uint32_t i = 0; i < point_count; ++i)
    {
        uint8_t flag = flags[i];
        if (flag & GLYF_FLAG_Y_SHORT)
        {
            if (position == data_size)
                return false;
            int32_t delta = data[position++];
            coordinate += flag & GLYF_FLAG_Y_SAME ? delta : -delta;
        }
        else if (!(flag & GLYF_FLAG_Y_SAME))
        {
            if (data_size - position < 2)
                return false;
            coordinate += ReadS16(data + position);
            position += 2;
        }
        result->m_Points[i].m_Y = coordinate;
    }
    return BuildSimpleContours(result->m_Points, ends.Begin(), contour_count, result->m_Commands);
}

static FontOutlinePoint TransformPoint(FontOutlinePoint point, float x_scale, float scale_01, float scale_10, float y_scale, float dx, float dy)
{
    FontOutlinePoint result = { point.m_X * x_scale + point.m_Y * scale_10 + dx,
                                point.m_X * scale_01 + point.m_Y * y_scale + dy };
    return result;
}

static bool DecodeGlyf(FontTrueType* font, uint32_t glyph_index, uint32_t depth, FontTrueTypeGlyf* result);

static bool DecodeCompositeGlyf(FontTrueType* font, const uint8_t* data, uint32_t data_size, uint32_t depth, FontTrueTypeGlyf* result)
{
    // Decode each component separately, apply its transform, then append it.
    // Point-number arguments attach a transformed component point to a point
    // already emitted by the compound glyph.
    uint32_t position = GLYF_HEADER_SIZE;
    uint16_t flags;
    do
    {
        if (data_size - position < 4)
            return false;
        flags = ReadU16(data + position);
        uint32_t component_glyph = ReadU16(data + position + 2);
        position += 4;
        int32_t arg1;
        int32_t arg2;
        if (flags & GLYF_COMPOSITE_WORD_ARGUMENTS)
        {
            if (data_size - position < 4)
                return false;
            if (flags & GLYF_COMPOSITE_ARGUMENTS_ARE_XY)
            {
                arg1 = ReadS16(data + position);
                arg2 = ReadS16(data + position + 2);
            }
            else
            {
                arg1 = ReadU16(data + position);
                arg2 = ReadU16(data + position + 2);
            }
            position += 4;
        }
        else
        {
            if (data_size - position < 2)
                return false;
            if (flags & GLYF_COMPOSITE_ARGUMENTS_ARE_XY)
            {
                arg1 = (int8_t)data[position];
                arg2 = (int8_t)data[position + 1];
            }
            else
            {
                arg1 = data[position];
                arg2 = data[position + 1];
            }
            position += 2;
        }

        float x_scale = 1.0f;
        float scale_01 = 0.0f;
        float scale_10 = 0.0f;
        float y_scale = 1.0f;
        if (flags & GLYF_COMPOSITE_UNIFORM_SCALE)
        {
            if (data_size - position < 2)
                return false;
            x_scale = y_scale = ReadS16(data + position) / F2DOT14_SCALE;
            position += 2;
        }
        else if (flags & GLYF_COMPOSITE_SEPARATE_XY_SCALE)
        {
            if (data_size - position < 4)
                return false;
            x_scale = ReadS16(data + position) / F2DOT14_SCALE;
            y_scale = ReadS16(data + position + 2) / F2DOT14_SCALE;
            position += 4;
        }
        else if (flags & GLYF_COMPOSITE_MATRIX)
        {
            if (data_size - position < 8)
                return false;
            x_scale = ReadS16(data + position) / F2DOT14_SCALE;
            scale_01 = ReadS16(data + position + 2) / F2DOT14_SCALE;
            scale_10 = ReadS16(data + position + 4) / F2DOT14_SCALE;
            y_scale = ReadS16(data + position + 6) / F2DOT14_SCALE;
            position += 8;
        }

        FontTrueTypeGlyf component;
        if (!DecodeGlyf(font, component_glyph, depth + 1, &component))
            return false;
        float dx = 0.0f;
        float dy = 0.0f;
        if (flags & GLYF_COMPOSITE_ARGUMENTS_ARE_XY)
        {
            FontOutlinePoint offset = { (float)arg1, (float)arg2 };
            if ((flags & GLYF_COMPOSITE_SCALED_OFFSET) && !(flags & GLYF_COMPOSITE_UNSCALED_OFFSET))
                offset = TransformPoint(offset, x_scale, scale_01, scale_10, y_scale, 0.0f, 0.0f);
            dx = offset.m_X;
            dy = offset.m_Y;
        }
        else
        {
            if ((uint32_t)arg1 >= result->m_Points.Size() || (uint32_t)arg2 >= component.m_Points.Size())
                return false;
            FontOutlinePoint component_point = TransformPoint(ToOutlinePoint(component.m_Points[arg2]), x_scale, scale_01, scale_10, y_scale, 0.0f, 0.0f);
            dx = result->m_Points[arg1].m_X - component_point.m_X;
            dy = result->m_Points[arg1].m_Y - component_point.m_Y;
        }

        for (uint32_t i = 0; i < component.m_Commands.Size(); ++i)
        {
            FontOutlineCommand command = component.m_Commands[i];
            uint32_t           point_count = GetOutlinePointCount(command.m_Type);
            for (uint32_t j = 0; j < point_count; ++j)
                command.m_Points[j] = TransformPoint(command.m_Points[j], x_scale, scale_01, scale_10, y_scale, dx, dy);
            if (result->m_Commands.Full())
                result->m_Commands.OffsetCapacity(OUTLINE_ARRAY_GROWTH);
            result->m_Commands.Push(command);
        }
        for (uint32_t i = 0; i < component.m_Points.Size(); ++i)
        {
            FontOutlinePoint  point = TransformPoint(ToOutlinePoint(component.m_Points[i]), x_scale, scale_01, scale_10, y_scale, dx, dy);
            FontTrueTypePoint transformed = { point.m_X, point.m_Y, component.m_Points[i].m_OnCurve };
            if (result->m_Points.Full())
                result->m_Points.OffsetCapacity(OUTLINE_ARRAY_GROWTH);
            result->m_Points.Push(transformed);
        }
    } while (flags & GLYF_COMPOSITE_MORE_COMPONENTS);
    return true;
}

static bool DecodeGlyf(FontTrueType* font, uint32_t glyph_index, uint32_t depth, FontTrueTypeGlyf* result)
{
    if (depth == MAX_GLYF_COMPOSITE_DEPTH)
        return false;
    const uint8_t* data;
    uint32_t       data_size;
    if (!GetGlyphData(font, glyph_index, &data, &data_size))
        return false;
    if (data_size == 0)
        return true;
    if (data_size < GLYF_HEADER_SIZE)
        return false;
    int16_t contour_count = ReadS16(data);
    if (contour_count >= 0)
        return DecodeSimpleGlyf(data, data_size, contour_count, result);
    return DecodeCompositeGlyf(font, data, data_size, depth, result);
}

FontResult FontTrueTypeGetGlyphOutline(FontTrueType* font, uint32_t glyph_index, FontOutline* outline)
{
    // Both outline backends return the same owned, unscaled representation.
    memset(outline, 0, sizeof(*outline));
    if (font->m_OutlineType == FONT_OUTLINE_TYPE_GLYF)
    {
        FontTrueTypeGlyf glyf;
        if (!DecodeGlyf(font, glyph_index, 0, &glyf))
            return FONT_RESULT_ERROR;
        if (glyf.m_Commands.Empty())
            return FONT_RESULT_OK;
        outline->m_CommandCount = glyf.m_Commands.Size();
        outline->m_Commands = (FontOutlineCommand*)malloc(sizeof(FontOutlineCommand) * outline->m_CommandCount);
        memcpy(outline->m_Commands, glyf.m_Commands.Begin(), sizeof(FontOutlineCommand) * outline->m_CommandCount);
        return FONT_RESULT_OK;
    }
    FontTrueTypeOutlineBuilder builder = {};
    if (!InterpretCharString(font, glyph_index, &builder, 0))
        return FONT_RESULT_ERROR;
    if (builder.m_Commands.Empty())
        return FONT_RESULT_OK;
    outline->m_CommandCount = builder.m_Commands.Size();
    outline->m_Commands = (FontOutlineCommand*)malloc(sizeof(FontOutlineCommand) * outline->m_CommandCount);
    memcpy(outline->m_Commands, builder.m_Commands.Begin(), sizeof(FontOutlineCommand) * outline->m_CommandCount);
    return FONT_RESULT_OK;
}

uint32_t FontTrueTypeGetGlyphIndex(FontTrueType* font, uint32_t codepoint)
{
    // SelectCmap chose one supported subtable at load time. Decode it directly
    // from borrowed data to avoid a per-font Unicode mapping allocation.
    const uint8_t* cmap = font->m_FontData + font->m_CmapOffset;
    uint16_t       format = ReadU16(cmap);
    if (format == CMAP_FORMAT_BYTE_ENCODING)
        return codepoint < CMAP_FORMAT_0_ENTRY_COUNT && CMAP_FORMAT_0_HEADER_SIZE + codepoint < font->m_CmapSize
                   ? cmap[CMAP_FORMAT_0_HEADER_SIZE + codepoint]
                   : 0;
    if (format == CMAP_FORMAT_TRIMMED_TABLE)
    {
        if (font->m_CmapSize < CMAP_FORMAT_6_HEADER_SIZE)
            return 0;
        uint32_t first = ReadU16(cmap + CMAP_FORMAT_6_FIRST_CODE_OFFSET);
        uint32_t count = ReadU16(cmap + CMAP_FORMAT_6_ENTRY_COUNT_OFFSET);
        if (codepoint < first || codepoint - first >= count)
            return 0;
        uint32_t offset = CMAP_FORMAT_6_HEADER_SIZE + (codepoint - first) * 2;
        return offset + 2 <= font->m_CmapSize ? ReadU16(cmap + offset) : 0;
    }
    if (format == CMAP_FORMAT_SEGMENTED_COVERAGE || format == CMAP_FORMAT_MANY_TO_ONE_RANGE)
    {
        if (font->m_CmapSize < CMAP_FORMAT_12_HEADER_SIZE)
            return 0;
        uint32_t count = ReadU32(cmap + CMAP_FORMAT_12_GROUP_COUNT_OFFSET);
        if (count > (font->m_CmapSize - CMAP_FORMAT_12_HEADER_SIZE) / CMAP_FORMAT_12_GROUP_SIZE)
            return 0;
        uint32_t low = 0;
        uint32_t high = count;
        while (low < high)
        {
            uint32_t       middle = low + (high - low) / 2;
            const uint8_t* group = cmap + CMAP_FORMAT_12_HEADER_SIZE + middle * CMAP_FORMAT_12_GROUP_SIZE;
            uint32_t       first = ReadU32(group);
            uint32_t       last = ReadU32(group + CMAP_GROUP_END_CODE_OFFSET);
            if (codepoint < first)
                high = middle;
            else if (codepoint > last)
                low = middle + 1;
            else
                return format == CMAP_FORMAT_SEGMENTED_COVERAGE
                           ? ReadU32(group + CMAP_GROUP_START_GLYPH_OFFSET) + codepoint - first
                           : ReadU32(group + CMAP_GROUP_START_GLYPH_OFFSET);
        }
        return 0;
    }
    if (format == CMAP_FORMAT_SEGMENT_MAPPING && codepoint <= MAX_BMP_CODEPOINT && font->m_CmapSize >= CMAP_FORMAT_4_MIN_SIZE)
    {
        uint32_t segment_count = ReadU16(cmap + CMAP_FORMAT_4_SEGMENT_COUNT_X2_OFFSET) / 2;
        uint32_t end_codes = CMAP_FORMAT_4_HEADER_SIZE;
        uint32_t start_codes = end_codes + segment_count * 2 + 2;
        uint32_t deltas = start_codes + segment_count * 2;
        uint32_t range_offsets = deltas + segment_count * 2;
        if (range_offsets + segment_count * 2 > font->m_CmapSize)
            return 0;
        for (uint32_t i = 0; i < segment_count; ++i)
        {
            uint32_t end = ReadU16(cmap + end_codes + i * 2);
            if (codepoint > end)
                continue;
            uint32_t start = ReadU16(cmap + start_codes + i * 2);
            if (codepoint < start)
                return 0;
            int32_t  delta = ReadS16(cmap + deltas + i * 2);
            uint32_t range_offset = ReadU16(cmap + range_offsets + i * 2);
            if (range_offset == 0)
                return (codepoint + delta) & MAX_BMP_CODEPOINT;
            uint32_t address = range_offsets + i * 2 + range_offset + (codepoint - start) * 2;
            if (address + 2 > font->m_CmapSize)
                return 0;
            uint32_t glyph = ReadU16(cmap + address);
            return glyph ? (glyph + delta) & MAX_BMP_CODEPOINT : 0;
        }
    }
    return 0;
}

float FontTrueTypeGetScaleFromSize(FontTrueType* font, uint32_t size)
{
    return (float)size / font->m_UnitsPerEm;
}

bool FontTrueTypeGetVerticalMetrics(FontTrueType* font, int32_t* ascent, int32_t* descent, int32_t* line_gap)
{
    *ascent = font->m_Ascent;
    *descent = font->m_Descent;
    *line_gap = font->m_LineGap;
    return true;
}

void FontTrueTypeGetGlyphHMetrics(FontTrueType* font, uint32_t glyph_index, int32_t* advance, int32_t* left_bearing)
{
    const uint8_t* hmtx = font->m_FontData + font->m_HmtxOffset;
    uint32_t       metric = glyph_index < font->m_NumHMetrics ? glyph_index : font->m_NumHMetrics - 1;
    *advance = ReadU16(hmtx + metric * 4);
    if (glyph_index < font->m_NumHMetrics)
        *left_bearing = ReadS16(hmtx + metric * 4 + 2);
    else
        *left_bearing = ReadS16(hmtx + font->m_NumHMetrics * 4 + (glyph_index - font->m_NumHMetrics) * 2);
}

FontOutlineType FontTrueTypeGetOutlineType(FontTrueType* font)
{
    return font->m_OutlineType;
}

bool FontTrueTypeGetGlyphBox(FontTrueType* font, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    if (font->m_OutlineType == FONT_OUTLINE_TYPE_GLYF)
    {
        const uint8_t* data;
        uint32_t data_size;
        if (!GetGlyphData(font, glyph_index, &data, &data_size) || data_size < GLYF_HEADER_SIZE)
            return false;

        // Every non-empty glyf record stores its bounds in the glyph header,
        // including composite glyphs. Reading them avoids decoding the points.
        *x0 = ReadS16(data + GLYF_X_MIN_OFFSET);
        *y0 = ReadS16(data + GLYF_Y_MIN_OFFSET);
        *x1 = ReadS16(data + GLYF_X_MAX_OFFSET);
        *y1 = ReadS16(data + GLYF_Y_MAX_OFFSET);
        return true;
    }

    // CFF charstrings have no per-glyph bounds field. Interpret drawing
    // commands directly into an exact bounds accumulator without allocating
    // or copying a temporary outline.
    FontOutlineBounds bounds = {};
    FontTrueTypeOutlineBuilder builder = {};
    builder.m_Bounds = &bounds;
    if (!InterpretCharString(font, glyph_index, &builder, 0))
        return false;

    float fx0, fy0, fx1, fy1;
    bool  has_bounds = FontOutlineBoundsGet(&bounds, &fx0, &fy0, &fx1, &fy1);
    if (has_bounds)
    {
        *x0 = (int32_t)floorf(fx0);
        *y0 = (int32_t)floorf(fy0);
        *x1 = (int32_t)ceilf(fx1);
        *y1 = (int32_t)ceilf(fy1);
    }
    return has_bounds;
}
