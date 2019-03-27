#ifndef DM_STATIC_ASSERT_H
#define DM_STATIC_ASSERT_H

#define DM_STATIC_ASSERT(x, error) \
    do { \
        static const char error[(x)?1:-1] = {0};\
        (void) error;\
    } while(0)

#if defined(__clang_analyzer__)
    inline void analyzer_use_pointer(void** pp){
        *((uintptr_t)p) = (uintptr_t)1;
    }
    #define ANALYZE_USE_POINTER(_P) analyzer_use_pointer((void**)&(_P))
#else
    #define ANALYZE_USE_POINTER(_P)
#endif

#endif // DM_STATIC_ASSERT_H
