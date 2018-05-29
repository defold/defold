#include "zlib.h"

#include "../zlib/zlib.h"

#include <assert.h>

namespace dmZlib
{
    static Result ToResult(int e) {
        switch (e) {
        case Z_OK:
            return RESULT_OK;
        case Z_STREAM_END:
            return RESULT_STREAM_END;
        case Z_NEED_DICT:
            return RESULT_NEED_DICT;
        case Z_ERRNO:
            return RESULT_ERRNO;
        case Z_STREAM_ERROR:
            return RESULT_STREAM_ERROR;
        case Z_DATA_ERROR:
            return RESULT_DATA_ERROR;
        case Z_MEM_ERROR:
            return RESULT_MEM_ERROR;
        case Z_BUF_ERROR:
            return RESULT_BUF_ERROR;
        case Z_VERSION_ERROR:
            return RESULT_VERSION_ERROR;
        }
        return RESULT_UNKNOWN;
    }

    Result InflateBuffer(const void* buffer, uint32_t buffer_size, void* context, Writer writer)
    {
        z_stream strm;
        unsigned char out[16384];

        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;

        int ret = inflateInit2(&strm, MAX_WBITS + 32);
        if (ret != Z_OK)
            return ToResult(ret);

        strm.avail_in = buffer_size;
        strm.next_in = (const uint8_t*) buffer;

        do {
            strm.avail_out = sizeof(out);
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);
            if (ret < 0 || ret == Z_NEED_DICT) {
                (void)inflateEnd(&strm);
                return RESULT_DATA_ERROR;
            }
            uint32_t have = sizeof(out) - strm.avail_out;
            if (!writer(context, out, have)) {
                (void)inflateEnd(&strm);
                return ToResult(Z_ERRNO);
            }
        } while (strm.avail_out == 0);

        inflateEnd(&strm);
        return ret == Z_STREAM_END ? RESULT_OK : RESULT_DATA_ERROR;
    }

    Result DeflateBuffer(const void* buffer, uint32_t buffer_size, int level, void* context, Writer writer)
    {
        int ret, flush;
        unsigned have;
        z_stream strm;
        unsigned char out[16384];

        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        ret = deflateInit(&strm, level);
        if (ret != Z_OK)
            return ToResult(ret);

        strm.avail_in = buffer_size;
        flush = Z_FINISH;
        strm.next_in = (const uint8_t*) buffer;

        do {
            strm.avail_out = sizeof(out);
            strm.next_out = out;

            ret = deflate(&strm, flush);
            assert(ret != Z_STREAM_ERROR);
            have = sizeof(out) - strm.avail_out;

            if (!writer(context, out, have)) {
                (void)deflateEnd(&strm);
                return RESULT_ERRNO;
            }
        } while (strm.avail_out == 0);

        assert(strm.avail_in == 0);
        assert(ret == Z_STREAM_END);

        (void)deflateEnd(&strm);
        return RESULT_OK;
    }

}
