#ifdef _WIN32

/*
 * NOTE: This function is private in ntdll and changed in window 2000
 * We assume version >= NT5, ie windows 2000.
 */
unsigned long __stdcall NtAllocateUuids(void* time  /* 8 bytes */,
                                        void* range /* 4 bytes */,
                                        void* sequence /* 4 bytes */,
                                        void* seed /* 6 bytes */);
#pragma comment(lib, "ntdll.lib")
#else
#include <uuid/uuid.h>
#endif

#include <stdint.h>
#include "uuid.h"

namespace dmUUID
{
#ifdef _WIN32
    void Generate(UUID* uuid)
    {
        static unsigned char seed[6] = { '\0' };
        uint8_t* p = &uuid->m_UUID[0];
        NtAllocateUuids(p, p+8, p+12, &seed[0] );
    }
#else
    void Generate(UUID* uuid)
    {
        uuid_generate(uuid->m_UUID);
    }
#endif
}
