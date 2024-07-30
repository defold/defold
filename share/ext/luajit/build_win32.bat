echo off

if not defined INCLUDE goto :FAIL
if not defined VCINSTALLDIR goto :FAIL
if not defined PLATFORM goto :FAIL

set TMP_TARGET=tmp_%PLATFORM%

set URL=https://github.com/LuaJIT/LuaJIT/archive/
set SHA1=04dca7911ea255f37be799c18d74c305b921c1a6
set SHA1_SHORT=04dca79
set VERSION=2.1.0-%SHA1_SHORT%
set PRODUCT=luajit
set TARGET_FILE=%PRODUCT%-%VERSION%
set PATCH_FILE=patch_%VERSION%

set ZIPFILENAME=%SHA1%.zip
set ZIPFILE=%URL%/%ZIPFILENAME%

set PACKAGEDIR=LuaJIT-%SHA1%%

echo "**************************************************"
echo "DOWNLOAD"
echo "**************************************************"

if exist %ZIPFILENAME% goto ZIPEXISTS
wget %ZIPFILE%
:ZIPEXISTS

echo "**************************************************"
echo "UNPACK"
echo "**************************************************"

if exist %TMP_TARGET% goto ZIPEXTRACTED
unzip -q %ZIPFILENAME% -d %TMP_TARGET%
echo "Deleting zip"
del %ZIPFILENAME%

if not exist %PATCH_FILE% goto ZIPEXTRACTED

echo "**************************************************"
echo "Applying patch $PATCH_FILE"
echo "**************************************************"

set FOLDER=%~dp0\%TMP_TARGET%\%PACKAGEDIR%\
set PATCH_PATH=%~dp0\%PATCH_FILE%

pushd %FOLDER%
git apply --unsafe-paths %PATCH_PATH%
popd


:ZIPEXTRACTED


echo "**************************************************"
echo "BUILD %PRODUCT% for %PLATFORM%"
echo "**************************************************"

set SOURCE_TARGET=%~dp0\%TMP_TARGET%\%PACKAGEDIR%\src

echo "SOURCE_TARGET:" %SOURCE_TARGET%
pushd %SOURCE_TARGET%


if "%platform%" == "x64" goto :BUILD_X64

:BUILD_X32
cmd "/C msvcbuild.bat nogc64 static dummy"
set TARGET_PLATFORM=win32
set BITDEPTH=32
goto :BUILD_DONE

:BUILD_X64
cmd "/C msvcbuild.bat static dummy"
set TARGET_PLATFORM=x86_64-win32
set BITDEPTH=64

:BUILD_DONE

rem host
popd



echo "**************************************************"
echo "Package %PRODUCT% for %TARGET_PLATFORM%"
echo "**************************************************"

set PACKAGE_NAME=%~dp0\package\%PRODUCT%-%VERSION%-%TARGET_PLATFORM%.tar.gz
set COMMON_PACKAGE_NAME=%~dp0\package\%PRODUCT%-%VERSION%-common.tar.gz
mkdir package

mkdir %TMP_TARGET%\package
pushd %TMP_TARGET%\package

    mkdir lib
    mkdir lib\%TARGET_PLATFORM%
    mkdir bin
    mkdir bin\%TARGET_PLATFORM%
    mkdir share
    mkdir share\luajit
    mkdir share\luajit\jit
    mkdir include
    mkdir include\luajit-2.1

    copy "%SOURCE_TARGET%\lua51.lib" lib\%TARGET_PLATFORM%\libluajit-5.1.lib
    copy "%SOURCE_TARGET%\luajit.exe" bin\%TARGET_PLATFORM%\luajit-%BITDEPTH%.exe

    copy "%SOURCE_TARGET%\lua.h" include\luajit-2.1
    copy "%SOURCE_TARGET%\lua.hpp" include\luajit-2.1
    copy "%SOURCE_TARGET%\luaconf.h" include\luajit-2.1
    copy "%SOURCE_TARGET%\luajit.h" include\luajit-2.1
    copy "%SOURCE_TARGET%\lualib.h" include\luajit-2.1
    copy "%SOURCE_TARGET%\lauxlib.h" include\luajit-2.1

    copy "%SOURCE_TARGET%\jit\*.lua" share\luajit\jit

    tar cfvz %PACKAGE_NAME% lib bin
    tar cfvz %COMMON_PACKAGE_NAME% include share

rem package
popd

echo "Wrote %PACKAGE_NAME%"
echo "Wrote %COMMON_PACKAGE_NAME%"



goto :END
:BAD
echo.
echo *******************************************************
echo *** Build FAILED -- Please check the error messages ***
echo *******************************************************
goto :END
:FAIL
echo To run this script you must open a "Native Tools Command Prompt for VS".
echo.
echo Either the x86 version, or x64.
:END
