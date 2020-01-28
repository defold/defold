#!/usr/bin/env bash

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source $SCRIPTDIR/common.sh

BUILDDIR=$PWD
BUILDDIR=$(path_to_posix $BUILDDIR)

function usage_private()  {
    echo "Supported platforms"
    echo " * arm64-nx64"
}

# a function to create a trampoline libraries for the configuration step
# since on switch it requires the nnMain function
function cmi_switch_stubs() {
    echo "cmi_switch_stubs Generating trampoline nnMain libraries"
    local platform=$1
    local outputdir=$2
    OUTPUT=$BUILDDIR/cmi_main_no_args.o
    FILE=$BUILDDIR/cmi_main_no_args.c
    CODE_NO_ARGS='extern int main();\nextern "C" void nnMain() {}\n'
    echo "Writing $FILE"
    printf "$CODE_NO_ARGS" > $FILE
    $CXX $CXXFLAGS -c $FILE -o $OUTPUT
    echo "Wrote $OUTPUT"

    # 
    OUTPUT=$BUILDDIR/cmi_stubs.o
    FILE=$BUILDDIR/cmi_stubs.c
    echo "Writing $FILE"
    printf '// stubs for unresolved libraries on Switch\n' > $FILE
    printf '#include <assert.h>\n' >> $FILE
    printf 'extern "C" int pipe(int pipefd[2]) { assert(false && "Defold stub function"); return 0; }\n' >> $FILE
    printf 'typedef int pid_t;\n' >> $FILE
    printf 'extern "C" pid_t fork(void) { assert(false && "Defold stub function"); return 0; }\n' >> $FILE
    printf 'extern "C" pid_t waitpid(pid_t pid, int *status, int options) { assert(false && "Defold stub function"); return 0; }\n' >> $FILE
    printf 'extern "C" void _exit(int status) { assert(false && "Defold stub function"); }\n' >> $FILE

    printf 'extern "C" int execvp(const char *file, char *const argv[]) { assert(false && "Defold stub function"); return 0; }\n' >> $FILE
    printf 'extern "C" int execv(const char *path, char *const argv[]) { assert(false && "Defold stub function"); return 0; }\n' >> $FILE
    printf 'typedef void (*sighandler_t)(int);\n' >> $FILE
    printf 'extern "C" sighandler_t signal(int signum, sighandler_t handler) { assert(false && "Defold stub function"); sighandler_t v; return v; }\n' >> $FILE
    $CXX $CXXFLAGS -c $FILE -o $OUTPUT
    echo "Wrote $OUTPUT"
}


function cmi_private()  {
    
    case $1 in
        arm64-nx64)

            #NINTENDO_SDK_ROOT should be setup by the environment
            [ ! -e "${NINTENDO_SDK_ROOT}" ] && echo "No Nintendo SDK found at ${NINTENDO_SDK_ROOT}" && exit 1

            BUILDTARGET="NX-NXFP2-a64"
            BUILDTYPE="Debug"
            TRIPLET="aarch64-nintendo-nx-elf"
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"

            NINTENDOSDKROOT=$(windows_path_to_posix $NINTENDO_SDK_ROOT)

            export INCLUDES="-I$NINTENDOSDKROOT/Common/Configs/Targets/$BUILDTARGET/Include -I$NINTENDOSDKROOT/Include"
            export PATH=$PATH:$NINTENDOSDKROOT/Tools/CommandLineTools:$NINTENDOSDKROOT/Compilers/NX/nx/aarch64/bin:
            export CPPFLAGS="--target=$TRIPLET -mcpu=cortex-a57+fp+simd+crypto+crc -fno-common -fno-short-enums -ffunction-sections -fdata-sections -fPIC -fdiagnostics-format=msvc"
            export CXXFLAGS="${CPPFLAGS} -std=gnu++98"
            export CFLAGS="${CPPFLAGS}"
            # not strictly necessary, since we only compile libraries
            export LDFLAGS="-Wl,-e,_main -L$NINTENDO_SDK_ROOT\Libraries\NX-NXFP2-a64\Debug -L$NINTENDO_SDK_ROOT\Libraries\NX-NXFP2-a64\Develop -nostartfiles -Wl,--gc-sections -Wl,--build-id=sha1 -Wl,-init=_init -Wl,-fini=_fini -Wl,-pie -Wl,-z,combreloc -Wl,-z,relro -Wl,--enable-new-dtags -Wl,-u,malloc -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,aligned_alloc -Wl,-u,free -fdiagnostics-format=msvc -Wl,-T $NINTENDOSDKROOT/Resources/SpecFiles/Application.aarch64.lp64.ldscript -Wl,--start-group $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/rocrt.o $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/nnApplication.o $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/libnn_init_memory.a $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/libnn_gll.a $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/libnn_gfx.a $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/libnn_mii_draw.a -Wl,--end-group -Wl,--start-group $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nss -Wl,--end-group $NINTENDOSDKROOT/Libraries/NX-NXFP2-a64/Develop/crtend.o $BUILDDIR/cmi_main_no_args.o $BUILDDIR/cmi_stubs.o -Wl,--dynamic-list=$NINTENDOSDKROOT/Resources/SpecFiles/ExportDynamicSymbol.lst"

            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$NINTENDOSDKROOT/Compilers/NX/nx/aarch64/bin/clang -E"
            export CC=$NINTENDOSDKROOT/Compilers/NX/nx/aarch64/bin/clang
            export CXX=$NINTENDOSDKROOT/Compilers/NX/nx/aarch64/bin/clang++
            export AR=$NINTENDOSDKROOT/Compilers/NX/nx/aarch64/bin/llvm-ar
            export RANLIB=$NINTENDOSDKROOT/Compilers/NX/nx/aarch64/bin/llvm-ranlib

            echo "PWD $PWD"

            # e.g. for generating
            cmi_switch_stubs $1 

            cmi_cross $1 arm-linux
            ;;

        *)
            echo "cmi_private: Unknown target $1" && exit 1
            ;;
    esac

}