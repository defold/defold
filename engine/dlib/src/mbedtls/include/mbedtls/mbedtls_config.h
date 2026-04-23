#ifndef __MBEDTLS_GENERAL_CONFIG__
#define __MBEDTLS_GENERAL_CONFIG__

#ifdef MBEDTLS_THREADING_ALT
    #define MBEDTLS_THREADING_C
    #include "mbedtls_config_full.h"
#else
    #include "mbedtls_config_min.h"
#endif
#endif
