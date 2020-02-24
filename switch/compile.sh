#!/usr/bin/env bash

set -e

BUILDTARGET="NX-NXFP2-a64"
BUILDTYPE="Debug"

TARGET=program

NINTENDO_SDK_ROOT=/c/Nintendo/SDK/NintendoSDK

INCLUDES="-I$NINTENDO_SDK_ROOT/Common/Configs/Targets/$BUILDTARGET/Include -I$NINTENDO_SDK_ROOT/Include"

CCFLAGS="-mcpu=cortex-a57+fp+simd+crypto+crc -std=gnu++14 -fno-common -fno-short-enums -ffunction-sections -fdata-sections -fPIC -fdiagnostics-format=msvc"

DEFINES="-DNN_ENABLE_LOG -DNN_ENABLE_ASSERT -DNN_ENABLE_ABORT_MESSAGE -DNN_NINTENDO_SDK -DNN_SDK_BUILD_DEBUG"



TRIPLET="aarch64-nintendo-nx-elf"

OBJECT_FILE="main.o"
#LINKFLAGS="-nostartfiles -Wl,--gc-sections -Wl,--build-id=sha1 -Wl,-init=_init,-fini=_fini -Wl,-pie -Wl,--export-dynamic,-z,combreloc,-z,relro,--enable-new-dtags -Wl,-u,malloc -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,aligned_alloc -Wl,-u,free -Wl,-T $NINTENDO_SDK_ROOT/Resources/SpecFiles/Application.aarch64.lp64.ldscript -fuse-ld=lld -Wl,--start-group $NINTENDO_SDK_ROOT/Libraries/$BUILDTARGET/$BUILDTYPE/rocrt.o $NINTENDO_SDK_ROOT/Libraries/$BUILDTARGET/$BUILDTYPE/nnApplication.o $OBJECT_FILE -Wl,--end-group -Wl,--start-group $NINTENDO_SDK_ROOT/Libraries/$BUILDTARGET/$BUILDTYPE/nnSdk.nss -Wl,--end-group $NINTENDO_SDK_ROOT/Libraries/$BUILDTARGET/$BUILDTYPE/crtend.o"

#DEPS="$OBJECT_FILE -llibnn_init_memory.a -llibnn_gll.a -llibnn_gfx.a -llibnn_mii_draw.a -lnnSdkEn.nss"
DEPS=""

LINKFLAGS="-nostartfiles -Wl,--gc-sections -Wl,--build-id=sha1 -Wl,-init=_init -Wl,-fini=_fini -Wl,-pie -Wl,-z,combreloc -Wl,-z,relro -Wl,--enable-new-dtags -Wl,-u,malloc -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,aligned_alloc -Wl,-u,free -fdiagnostics-format=msvc -Wl,-T C:/Nintendo/SDK/NintendoSDK/Resources/SpecFiles/Application.aarch64.lp64.ldscript -Wl,--start-group C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/rocrt.o C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/nnApplication.o C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/libnn_init_memory.a C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/libnn_gll.a C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/libnn_gfx.a C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/libnn_mii_draw.a -Wl,--end-group -Wl,--start-group C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nss -Wl,--end-group C:/Nintendo/SDK/NintendoSDK/Libraries/NX-NXFP2-a64/Develop/crtend.o"

LIBPATHS="-L$NINTENDO_SDK_ROOT/Libraries/$BUILDTARGET/$BUILDTYPE"

CLANG="$NINTENDO_SDK_ROOT/Compilers/NX/nx/aarch64/bin/clang++"
MAKENSO="$NINTENDO_SDK_ROOT\Tools\CommandLineTools\MakeNso\MakeNso.exe"
MAKEMETA="$NINTENDO_SDK_ROOT\Tools\CommandLineTools\MakeMeta\MakeMeta.exe"
AUTHORINGTOOL="$NINTENDO_SDK_ROOT\Tools\CommandLineTools\AuthoringTool\AuthoringTool.exe"

echo Compiling...
$CLANG --target=$TRIPLET $CCFLAGS $DEFINES $INCLUDES -c main.cpp -o $OBJECT_FILE

echo Linking...

#FOO="-Wl,--dynamic-list=C:/Nintendo/SDK/NintendoSDK/Resources/SpecFiles/ExportDynamicSymbol.lst -o C:\Nintendo\SDK\NintendoSDK\Samples\Sources\Applications\GfxSimple\Binaries\NX64\NX_Debug\GfxSimple.nspd_root"

$CLANG --verbose --target=$TRIPLET $LINKFLAGS -L"$NINTENDO_SDK_ROOT\Libraries\NX-NXFP2-a64\Debug" -L"$NINTENDO_SDK_ROOT\Libraries\NX-NXFP2-a64\Develop" -Wl,--start-group $OBJECT_FILE $DEPS -Wl,--end-group -Wl,--dynamic-list=C:/Nintendo/SDK/NintendoSDK/Resources/SpecFiles/ExportDynamicSymbol.lst -o $TARGET.nss

echo Make NSO

echo $MAKENSO $TARGET.nss $TARGET.nso

$MAKENSO $TARGET.nss $TARGET.nso

echo Make Meta

$MAKEMETA --desc $NINTENDO_SDK_ROOT/Resources/SpecFiles/Application.desc --meta $NINTENDO_SDK_ROOT/Resources/SpecFiles/Application.aarch64.lp64.nmeta -o $TARGET.npdm -d DefaultIs64BitInstruction=True -d DefaultProcessAddressSpace=AddressSpace64Bit


echo Creating code/ folder

# https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_170694395.html

CODEDIR=./$TARGET.nspd/program0.ncd/code
mkdir -p $CODEDIR

cp -v $TARGET.nso $CODEDIR/main
cp -v $TARGET.npdm $CODEDIR/main.npdm
cp -v $NINTENDO_SDK_ROOT/Libraries/NX-NXFP2-a64/Develop/nnrtld.nso $CODEDIR/rtld
cp -v $NINTENDO_SDK_ROOT/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nso $CODEDIR/sdk

echo Authoring

# https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_107320233.html
#mkdir ./data
$AUTHORINGTOOL createnspd -o $TARGET.nspd --meta $NINTENDO_SDK_ROOT/Resources/SpecFiles/Application.aarch64.lp64.nmeta --type Application --program ./$TARGET.nspd/program0.ncd/code . --utf8

