#ifndef DM_STATIC_ASSERT_H
#define DM_STATIC_ASSERT_H

#define DM_STATIC_ASSERT(x, error) \
    do { \
        static const char error[(x)?1:-1] = {0};\
        (void) error;\
    } while(0)

#endif // DM_STATIC_ASSERT_H
