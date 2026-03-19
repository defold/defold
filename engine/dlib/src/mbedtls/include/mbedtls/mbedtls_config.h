#ifndef __MBEDTLS_GENERAL_CONFIG__
#define __MBEDTLS_GENERAL_CONFIG__

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT
#endif

#ifndef DEFOLD_MBEDTLS_FAT
    #include "mbedtls_config_min.h"
#else
    #include "mbedtls_config_fat.h"
#endif
#endif