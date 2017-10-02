File embedding
--------------

Add feature embed and specify sources to be embedded with embed_source

    bld.new_task_gen(features = 'cxx cprogram embed', embed_source = 'data/test.embed', ...)

In your .cpp-file add the following symbols:

    extern unsigned char TEST_EMBED[];
    extern uint32_t TEST_EMBED_SIZE;

The symbol is the filename without path. Dots are replaced with underscore.


Memory profiling
----------------

Run dmengine_memprofile.sh instead of dmengine.

To enable tracing where every allocation is written to disk for analaysis:

    # DMMEMPROFILE_TRACE=1 dmengine_memprofile.sh

Create a trace report using memprofile.sh

    memprofile.sh `which dmengine` memprofile.trace > report.html

axTLS
-----

* The os_int.h is replaced by #include <stdint.h>
* Removed __stdcall on WIN32, see change in os_port.h
* Fixed issues in send_packet and basic_read in tls1.c
* Change from rand_r() to srand() in RNG_initialize() in crypto_misc.c

webp
-----

* replaced intN_t #define's in types.h with #include <stdint.h> as msvc now ships with one.
