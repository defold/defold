#ifndef DM_PATH_H
#define DM_PATH_H

#include <stdint.h>

/** Maximum path length convention. This size should large enough
 * such that path truncation never occurs in practice. No functions
 * in dmPath return error codes for that reason.
 */
#define DMPATH_MAX_PATH (1024)

namespace dmPath
{
#if defined(_WIN32) || defined(_WIN64)
    static const char* const PATH_CHARACTER = "\\";
    static const char* const PATH_SEPARATOR = ";";
#else
    static const char* const PATH_CHARACTER = "/";
    static const char* const PATH_SEPARATOR = ":";
#endif

    /**
     * Path normalization. Redundant and trailing slashes are
     * removed and backslashes are translated into forward slashes
     * @param path path to normalize
     * @param out out buffer
     * @param out_size out buffer size
     */
    void Normalize(const char* path, char* out, uint32_t out_size);

    /**
     * Get directory part of path. The output path is potentially
     * normalized, but not canonicalized, according to the Normalize() function.
     * @note internal buffer of DMPATH_MAX_PATH is used when
     * normalizing
     * @param path path to get directory of
     * @param out out buffer
     * @param out_size out buffer size
     */
    void Dirname(const char* path, char* out, uint32_t out_size);

    /**
     * Path concatenation function
     * @param path1 first path to concatenate
     * @param path2 second path to concatenate
     * @param out output path
     * @param out_size output path size
     */
    void Concat(const char* path1, const char* path2, char* out, uint32_t out_size);


}

#endif // DM_PATH_H
