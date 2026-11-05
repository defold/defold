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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dlib/lz4.h>
#include <lz4.h>
#define LZ4_HC_STATIC_LINKING_ONLY
#include <lz4hc.h>
#define LZ4F_STATIC_LINKING_ONLY
#include <lz4frame.h>

static void Check(bool success, const char* message)
{
    if (!success)
    {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static double Seconds()
{
    struct timespec now;
    Check(clock_gettime(CLOCK_MONOTONIC, &now) == 0, "clock_gettime failed");
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

struct Benchmark
{
    bool               m_Frame;
    char*              m_Input;
    char*              m_Compressed;
    char*              m_Decoded;
    int                m_InputSize;
    size_t             m_Capacity;
    size_t             m_CompressedSize;
    int                m_Level;
    LZ4_streamHC_t*    m_HCCompressor;
    LZ4F_cctx*         m_Compressor;
    LZ4F_dctx*         m_Decompressor;
    LZ4F_preferences_t m_Preferences;
};

static size_t Compress(Benchmark* benchmark)
{
    if (benchmark->m_Frame)
    {
        size_t result = LZ4F_compressFrame_usingCDict(benchmark->m_Compressor,
                                                      benchmark->m_Compressed, benchmark->m_Capacity,
                                                      benchmark->m_Input, benchmark->m_InputSize, NULL, &benchmark->m_Preferences);
        Check(!LZ4F_isError(result), LZ4F_getErrorName(result));
        return result;
    }
    if (benchmark->m_HCCompressor)
    {
        // Start an independent block while retaining the allocation for this worker.
        LZ4_resetStreamHC_fast(benchmark->m_HCCompressor, benchmark->m_Level);
        int size = LZ4_compress_HC_continue(benchmark->m_HCCompressor, benchmark->m_Input,
                                            benchmark->m_Compressed, benchmark->m_InputSize, benchmark->m_Capacity);
        Check(size > 0, "Reused-state compression failed");
        return size;
    }
    if (benchmark->m_Level != 9)
    {
        int size = LZ4_compress_HC(benchmark->m_Input, benchmark->m_Compressed,
                                   benchmark->m_InputSize, benchmark->m_Capacity, benchmark->m_Level);
        Check(size > 0, "Raw compression failed");
        return size;
    }
    int size;
    Check(dmLZ4::CompressBuffer(benchmark->m_Input, benchmark->m_InputSize,
                                benchmark->m_Compressed, &size) == dmLZ4::RESULT_OK, "Raw compression failed");
    return size;
}

static void Decompress(Benchmark* benchmark)
{
    if (benchmark->m_Frame)
    {
        size_t input_size = benchmark->m_CompressedSize;
        size_t output_size = benchmark->m_InputSize;
        size_t result = LZ4F_decompress(benchmark->m_Decompressor,
                                        benchmark->m_Decoded, &output_size, benchmark->m_Compressed, &input_size, NULL);
        Check(!LZ4F_isError(result) && result == 0 && input_size == benchmark->m_CompressedSize
              && output_size == (size_t)benchmark->m_InputSize, "Frame decompression failed");
        return;
    }
    int size;
    Check(dmLZ4::DecompressBuffer(benchmark->m_Compressed, benchmark->m_CompressedSize,
                                  benchmark->m_Decoded, benchmark->m_InputSize, &size) == dmLZ4::RESULT_OK
          && size == benchmark->m_InputSize, "Raw decompression failed");
}

int main(int argc, char** argv)
{
    Check(argc == 6 || argc == 7, "Usage: lz4_benchmark raw|raw-reuse|frame INPUT WORKERS SECONDS ROUNDS [LEVEL]");
    Check(strcmp(argv[1], "raw") == 0 || strcmp(argv[1], "raw-reuse") == 0 || strcmp(argv[1], "frame") == 0, "Unknown format");
    unsigned workers = (unsigned)atoi(argv[3]);
    double duration = atof(argv[4]);
    int rounds = atoi(argv[5]);
    Check(duration > 0 && rounds > 0, "SECONDS and ROUNDS must be positive");

    Benchmark benchmark = {};
    benchmark.m_Frame = strcmp(argv[1], "frame") == 0;
    benchmark.m_Level = argc == 7 ? atoi(argv[6]) : 9;
    Check(benchmark.m_Level >= 1 && benchmark.m_Level <= LZ4HC_CLEVEL_MAX, "LEVEL must be 1 to 12");
    Check(benchmark.m_Frame || workers == 0, "Raw blocks have no worker parameter");
    benchmark.m_Preferences.frameInfo.blockSizeID = LZ4F_max64KB;
    benchmark.m_Preferences.frameInfo.blockMode = LZ4F_blockLinked;
    benchmark.m_Preferences.compressionLevel = benchmark.m_Level;
#if LZ4_VERSION_NUMBER >= 11100
    Check(workers == 0 || LZ4F_getMaxNbWorkers() >= workers, "Thread support unavailable");
    benchmark.m_Preferences.nbWorkers = workers;
#else
    Check(workers == 0, "This LZ4 release has no frame worker API");
#endif

    FILE* input = fopen(argv[2], "rb");
    Check(input != NULL, "Could not open input");
    Check(fseek(input, 0, SEEK_END) == 0, "Could not seek input");
    long input_size = ftell(input);
    Check(input_size >= 0 && input_size <= LZ4_MAX_INPUT_SIZE, "Input exceeds raw-block limit");
    benchmark.m_InputSize = (int)input_size;
    rewind(input);
    benchmark.m_Input = (char*)malloc(input_size + 1);
    benchmark.m_Decoded = (char*)malloc(input_size + 1);
    Check(benchmark.m_Input != NULL && benchmark.m_Decoded != NULL, "Allocation failed");
    Check(fread(benchmark.m_Input, 1, input_size, input) == (size_t)input_size, "Could not read input");
    fclose(input);

    int raw_capacity;
    Check(dmLZ4::MaxCompressedSize(input_size, &raw_capacity) == dmLZ4::RESULT_OK, "Invalid raw size");
    benchmark.m_Capacity = benchmark.m_Frame
        ? LZ4F_compressFrameBound(input_size, &benchmark.m_Preferences) : (size_t)raw_capacity;
    Check(!LZ4F_isError(benchmark.m_Capacity), "Invalid frame size");
    benchmark.m_Compressed = (char*)malloc(benchmark.m_Capacity);
    Check(benchmark.m_Compressed != NULL, "Allocation failed");
    if (benchmark.m_Frame)
    {
        Check(!LZ4F_isError(LZ4F_createCompressionContext(&benchmark.m_Compressor, LZ4F_VERSION)), "Context allocation failed");
        Check(!LZ4F_isError(LZ4F_createDecompressionContext(&benchmark.m_Decompressor, LZ4F_VERSION)), "Context allocation failed");
    }
    if (strcmp(argv[1], "raw-reuse") == 0)
    {
        benchmark.m_HCCompressor = LZ4_createStreamHC();
        Check(benchmark.m_HCCompressor != NULL, "Context allocation failed");
    }

    double start = Seconds();
    benchmark.m_CompressedSize = Compress(&benchmark);
    double first_ms = (Seconds() - start) * 1000.0;
    Decompress(&benchmark);
    Check(memcmp(benchmark.m_Input, benchmark.m_Decoded, input_size) == 0, "Round trip differs");

    if (benchmark.m_HCCompressor)
    {
        // Seed the state with a different file sharing our prefix, then verify the reset removes its history.
        char* reference = (char*)malloc(benchmark.m_Capacity);
        int previous_size = benchmark.m_InputSize < 65536 ? benchmark.m_InputSize : 65536;
        char* previous = (char*)malloc(previous_size + 1);
        Check(reference != NULL && previous != NULL, "Allocation failed");
        memcpy(previous, benchmark.m_Input, previous_size);
        int reference_size = LZ4_compress_HC(benchmark.m_Input, reference, benchmark.m_InputSize,
                                             benchmark.m_Capacity, benchmark.m_Level);
        Check(reference_size > 0, "Reference compression failed");
        LZ4_resetStreamHC_fast(benchmark.m_HCCompressor, benchmark.m_Level);
        Check(LZ4_compress_HC_continue(benchmark.m_HCCompressor, previous, benchmark.m_Compressed,
                                       previous_size, benchmark.m_Capacity) > 0, "Previous-file compression failed");
        Check(Compress(&benchmark) == (size_t)reference_size
              && memcmp(reference, benchmark.m_Compressed, reference_size) == 0, "Reused block differs from one-shot block");
        Decompress(&benchmark);
        Check(memcmp(benchmark.m_Input, benchmark.m_Decoded, input_size) == 0, "Reused block needs previous history");
        free(previous);
        free(reference);
    }

#if LZ4_VERSION_NUMBER >= 11100
    // Scheduling and worker count must not change the compressed frame bytes.
    if (workers > 0)
    {
        LZ4F_cctx* reference;
        Check(!LZ4F_isError(LZ4F_createCompressionContext(&reference, LZ4F_VERSION)), "Context allocation failed");
        char* compressed = (char*)malloc(benchmark.m_Capacity);
        Check(compressed != NULL, "Allocation failed");
        LZ4F_preferences_t preferences = benchmark.m_Preferences;
        preferences.nbWorkers = 1;
        size_t size = LZ4F_compressFrame_usingCDict(reference, compressed, benchmark.m_Capacity,
                                                    benchmark.m_Input, input_size, NULL, &preferences);
        Check(size == benchmark.m_CompressedSize && memcmp(compressed, benchmark.m_Compressed, size) == 0,
              "Frame bytes depend on worker count");
        LZ4F_freeCompressionContext(reference);
        free(compressed);
    }
#endif

    for (int round = 0; round < rounds; ++round)
    {
        unsigned compressions = 0;
        start = Seconds();
        double elapsed;
        do
        {
            Check(Compress(&benchmark) == benchmark.m_CompressedSize, "Compressed size changed");
            ++compressions;
            elapsed = Seconds() - start;
        } while (elapsed < duration);
        double compression_mbps = (double)input_size * compressions / elapsed / 1e6;

        unsigned decompressions = 0;
        start = Seconds();
        do
        {
            Decompress(&benchmark);
            ++decompressions;
            elapsed = Seconds() - start;
        } while (elapsed < duration);
        Check(memcmp(benchmark.m_Input, benchmark.m_Decoded, input_size) == 0, "Round trip differs");
        double decompression_mbps = (double)input_size * decompressions / elapsed / 1e6;
        size_t context_bytes = benchmark.m_Frame ? 0 : LZ4_sizeofStateHC();
#if LZ4_VERSION_NUMBER >= 11100
        if (benchmark.m_HCCompressor)
        {
            context_bytes = LZ4_STREAMHC_EXTSIZE;
        }
        if (benchmark.m_Frame)
        {
            context_bytes = LZ4F_cctx_size(benchmark.m_Compressor);
        }
#endif
        printf("{\"version\":\"%s\",\"format\":\"%s\",\"workers\":%u,\"level\":%d,\"round\":%d,"
               "\"input_bytes\":%d,\"compressed_bytes\":%zu,\"compress_MBps\":%.3f,"
               "\"decompress_MBps\":%.3f,\"first_compress_ms\":%.3f,\"context_bytes\":%zu}\n",
               LZ4_versionString(), argv[1], workers, benchmark.m_Level, round, benchmark.m_InputSize,
               benchmark.m_CompressedSize, compression_mbps, decompression_mbps, first_ms, context_bytes);
    }

    LZ4F_freeCompressionContext(benchmark.m_Compressor);
    LZ4F_freeDecompressionContext(benchmark.m_Decompressor);
    LZ4_freeStreamHC(benchmark.m_HCCompressor);
    free(benchmark.m_Compressed);
    free(benchmark.m_Decoded);
    free(benchmark.m_Input);
    return 0;
}
