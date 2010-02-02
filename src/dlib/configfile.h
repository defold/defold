#include <stdint.h>

namespace dmConfigFile
{

    typedef struct Config* HConfig;

    enum Result
    {
	RESULT_OK = 0,
	RESULT_FILE_NOT_FOUND = -1,
	RESULT_LITERAL_TOO_LONG = -2,
	RESULT_SYNTAX_ERROR = -3,
	RESULT_UNEXPECTED_EOF = -4,
    };

    Result Load(const char* file_name, HConfig* config);
    void Delete(HConfig config);
    const char* GetString(HConfig config, const char* key, const char* default_value);
    int32_t GetInt(HConfig config, const char* key, int32_t default_value);
}



