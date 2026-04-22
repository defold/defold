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

#include <assert.h>
#include "zlib.h"
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../zip/miniz_rename.h"
#include "../zip/miniz.h"
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#undef MINIZ_NO_STDIO
#undef MINIZ_NO_ARCHIVE_APIS

namespace dmZlib
{
    static uint16_t ReadLE16(const uint8_t* buffer)
    {
        return (uint16_t)(buffer[0] | (buffer[1] << 8));
    }

    static uint32_t ReadLE32(const uint8_t* buffer)
    {
        return (uint32_t)(buffer[0] | (buffer[1] << 8) |
                (buffer[2] << 16) | (buffer[3] << 24));
    }

    static bool IsGZip(const uint8_t* buffer, uint32_t buffer_size)
    {
        return buffer_size >= 2 && buffer[0] == 0x1f && buffer[1] == 0x8b;
    }

    static bool ParseGZipHeader(const uint8_t* buffer, uint32_t buffer_size,
            const uint8_t** payload, uint32_t* payload_size,
            uint32_t* expected_crc32, uint32_t* expected_size)
    {
        const uint8_t FHCRC          = 1 << 1;
        const uint8_t FEXTRA         = 1 << 2;
        const uint8_t FNAME          = 1 << 3;
        const uint8_t FCOMMENT       = 1 << 4;
        const uint8_t RESERVED_FLAGS = 0xE0;

        if (buffer_size < 18 || !IsGZip(buffer, buffer_size))
            return false;

        if (buffer[2] != 8 || (buffer[3] & RESERVED_FLAGS) != 0)
            return false;

        uint32_t pos = 10;
        uint32_t trailer_offset = buffer_size - 8;
        uint8_t flags = buffer[3];

        if (flags & FEXTRA)
        {
            if (pos + 2 > trailer_offset)
                return false;

            uint32_t extra_len = ReadLE16(buffer + pos);
            pos += 2;
            if (pos + extra_len > trailer_offset)
                return false;
            pos += extra_len;
        }

        if (flags & FNAME)
        {
            while (pos < trailer_offset && buffer[pos] != 0)
                ++pos;
            if (pos >= trailer_offset)
                return false;
            ++pos;
        }

        if (flags & FCOMMENT)
        {
            while (pos < trailer_offset && buffer[pos] != 0)
                ++pos;
            if (pos >= trailer_offset)
                return false;
            ++pos;
        }

        if (flags & FHCRC)
        {
            if (pos + 2 > trailer_offset)
                return false;
            pos += 2;
        }

        if (pos > trailer_offset)
            return false;

        *payload = buffer + pos;
        *payload_size = trailer_offset - pos;
        *expected_crc32 = ReadLE32(buffer + trailer_offset);
        *expected_size = ReadLE32(buffer + trailer_offset + 4);
        return true;
    }

    static Result ToResult(int e) {
        switch (e) {
        case MZ_OK:
            return RESULT_OK;
        case MZ_STREAM_END:
            return RESULT_STREAM_END;
        case MZ_NEED_DICT:
            return RESULT_NEED_DICT;
        case MZ_ERRNO:
            return RESULT_ERRNO;
        case MZ_STREAM_ERROR:
            return RESULT_STREAM_ERROR;
        case MZ_DATA_ERROR:
            return RESULT_DATA_ERROR;
        case MZ_MEM_ERROR:
            return RESULT_MEM_ERROR;
        case MZ_BUF_ERROR:
            return RESULT_BUF_ERROR;
        case MZ_VERSION_ERROR:
            return RESULT_VERSION_ERROR;
        case MZ_PARAM_ERROR:
            return RESULT_STREAM_ERROR;
        }
        return RESULT_UNKNOWN;
    }

    Result InflateBuffer(const void* buffer, uint32_t buffer_size, void* context, Writer writer)
    {
        mz_stream strm = {};
        unsigned char out[16384];
        const uint8_t* input = (const uint8_t*)buffer;
        uint32_t input_size = buffer_size;
        uint32_t expected_crc32 = 0;
        uint32_t expected_size = 0;
        uint32_t crc32 = MZ_CRC32_INIT;
        bool is_gzip = IsGZip(input, input_size);

        if (is_gzip)
        {
            if (!ParseGZipHeader(input, input_size, &input, &input_size,
                    &expected_crc32, &expected_size))
                return RESULT_DATA_ERROR;
        }

        int ret = mz_inflateInit2(&strm,
                is_gzip ? -MZ_DEFAULT_WINDOW_BITS : MZ_DEFAULT_WINDOW_BITS);
        if (ret != MZ_OK)
            return ToResult(ret);

        strm.avail_in = input_size;
        strm.next_in = (const unsigned char*)input;

        do {
            strm.avail_out = sizeof(out);
            strm.next_out = out;

            ret = mz_inflate(&strm, MZ_NO_FLUSH);
            assert(ret != MZ_STREAM_ERROR);
            if (ret < 0 || ret == MZ_NEED_DICT) {
                (void)mz_inflateEnd(&strm);
                return RESULT_DATA_ERROR;
            }
            uint32_t have = sizeof(out) - strm.avail_out;
            crc32 = (uint32_t)mz_crc32(crc32, out, have);
            if (!writer(context, out, have)) {
                (void)mz_inflateEnd(&strm);
                return RESULT_ERRNO;
            }
        } while (ret != MZ_STREAM_END);

        (void)mz_inflateEnd(&strm);

        if (is_gzip && (crc32 != expected_crc32 ||
                (uint32_t)strm.total_out != expected_size))
            return RESULT_DATA_ERROR;

        return RESULT_OK;
    }

    Result DeflateBuffer(const void* buffer, uint32_t buffer_size, int level, void* context, Writer writer)
    {
        int ret;
        unsigned have;
        mz_stream strm = {};
        unsigned char out[16384];

        ret = mz_deflateInit2(&strm, level, MZ_DEFLATED,
                MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY);
        if (ret != MZ_OK)
            return ToResult(ret);

        strm.avail_in = buffer_size;
        strm.next_in = (const unsigned char*)buffer;

        do {
            strm.avail_out = sizeof(out);
            strm.next_out = out;

            ret = mz_deflate(&strm, MZ_FINISH);
            assert(ret != MZ_STREAM_ERROR);
            if (ret != MZ_OK && ret != MZ_STREAM_END) {
                (void)mz_deflateEnd(&strm);
                return ToResult(ret);
            }
            have = sizeof(out) - strm.avail_out;

            if (!writer(context, out, have)) {
                (void)mz_deflateEnd(&strm);
                return RESULT_ERRNO;
            }
        } while (ret != MZ_STREAM_END);

        assert(strm.avail_in == 0);
        assert(ret == MZ_STREAM_END);

        (void)mz_deflateEnd(&strm);
        return RESULT_OK;
    }

}
