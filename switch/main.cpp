#include <stdio.h>

#include <cstdlib>
#include <nn/nn_Common.h>
#include <nn/nn_Abort.h>
#include <nn/nn_Log.h>
#include <nn/init.h>
#include <nn/os.h>

static bool is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

extern "C" void nnMain()
{
    printf("HELLO WORLD!  big endian? %d\n", is_big_endian());
}