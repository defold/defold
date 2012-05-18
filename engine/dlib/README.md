File embedding
--------------

Add feature embed and specify sources to be embedded with embed_source
bld.new_task_gen(features = 'cxx cprogram embed', embed_source = 'data/test.embed', ...)

In your .cpp-file add the following symbols:

    extern unsigned char TEST_EMBED[];
    extern uint32_t TEST_EMBED_SIZE;

The symbol is the filename without path. Dots are replaced with underscore.
