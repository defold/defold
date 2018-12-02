#pragma once

#if defined(_MSC_VER)
    #include <string.h>
    #if !defined(strdup)
        #define strdup _strdup
    #endif

    #include <io.h>
    #if !defined(open)
        #define open _open
    #endif
    #if !defined(close)
        #define close _close
    #endif
    #if !defined(write)
        #define write _write
    #endif
    #if !defined(read)
        #define read _read
    #endif
    #if !defined(unlink)
        #define unlink _unlink
    #endif

    #include <direct.h>
    #if !defined(rmdir)
        #define rmdir _rmdir
    #endif
    #if !defined(mkdir)
        #define mkdir _mkdir
    #endif
    #if !defined(chdir)
        #define chdir _chdir
    #endif

    #include <stdio.h>
    #if !defined(fdopen)
        #define fdopen _fdopen
    #endif
#endif
