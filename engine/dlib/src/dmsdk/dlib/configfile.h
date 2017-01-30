#ifndef DMSDK_CONFIGFILE_H
#define DMSDK_CONFIGFILE_H

#include <stdint.h>

namespace dmConfigFile
{
    /**
     * Config file handle
     */
    typedef struct Config* HConfig;

    /**
     * Get config value as string
     * @param config Config file handle
     * @param key Key in format section.key (.key for no section)
     * @param default_value Default value to return if key isn't found
     * @return Found value or default value
     */
    const char* GetString(HConfig config, const char* key, const char* default_value);

    /**
     * Get config value as int. NOTE: default_value is returned for invalid integer values
     * @param config Config file handle
     * @param key Key (see GetString())
     * @param default_value Default value to return if key isn't found
     * @return Found value or default value
     */
    int32_t GetInt(HConfig config, const char* key, int32_t default_value);

    /**
     * Get config value as float. NOTE: default_value is returned for invalid float values
     * @param config Config file handle
     * @param key Key (see GetString())
     * @param default_value Default value to return if key isn't found
     * @return Found value or default value
     */
    float GetFloat(HConfig config, const char* key, float default_value);
}

#endif // DMSDK_CONFIGFILE_H

