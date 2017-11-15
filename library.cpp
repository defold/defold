
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//#if HAVE_VISIBILITY
//#define DM_EXPORTED_FUNC __attribute__((__visibility__("default")))
// #elif defined _MSC_VER
// #define DM_EXPORTED_FUNC __declspec(dllexport)
// #else
#define DM_EXPORTED_FUNC
// #endif

static uint32_t adler32(const void *buf, size_t buflength) {
     const uint8_t* buffer = (const uint8_t*)buf;

     uint32_t s1 = 1;
     uint32_t s2 = 0;

     for (size_t n = 0; n < buflength; n++) {
        s1 = (s1 + buffer[n]) % 65521;
        s2 = (s2 + s1) % 65521;
     }     
     return (s2 << 16) | s1;
}

extern "C" DM_EXPORTED_FUNC uint32_t GetHash(const char* str, size_t strlength)
{
	return adler32(str, strlength);
}

extern "C" DM_EXPORTED_FUNC void GetVersion(uint32_t* major, uint32_t* middle, uint32_t* minor)
{
	if (major)  *major =  1;
	if (middle)	*middle = 2;
	if (minor)	*minor = 118;
}
