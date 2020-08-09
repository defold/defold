@if not defined INCLUDE goto :FAIL
@if not defined NINTENDO_SDK_ROOT goto :FAIL

@setlocal
@rem ---- Host compiler ----
@set LJCOMPILE=cl /nologo /c /MD /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE
@set LJLINK=link /nologo
@set LJMT=mt /nologo
@set DASMDIR=..\dynasm
@set DASM=%DASMDIR%\dynasm.lua
@set ALL_LIB=lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c
@set GC64=-DLUAJIT_ENABLE_GC64
@set DASC=vm_arm64.dasc

@if "%1" neq "gc32" goto :NOGC32
@shift
@set GC64=
@set DASC=vm_arm.dasc
:NOGC32

%LJCOMPILE% host\minilua.c
@if errorlevel 1 goto :BAD
%LJLINK% /out:minilua.exe minilua.obj
@if errorlevel 1 goto :BAD
if exist minilua.exe.manifest^
  %LJMT% -manifest minilua.exe.manifest -outputresource:minilua.exe

@rem Check for 64 bit host compiler.
@minilua
@if not errorlevel 8 goto :FAIL

@set DASMFLAGS= -D ENDIAN_LE
minilua %DASM% -LN %DASMFLAGS% -o host\buildvm_arch.h %DASC%
@if errorlevel 1 goto :BAD

%LJCOMPILE% /I "." /I %DASMDIR% %GC64% -DLUAJIT_TARGET=LUAJIT_ARCH_ARM64 -DLUAJIT_OS=LUAJIT_OS_OTHER -DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI -DLJ_TARGET_NX host\buildvm*.c
@if errorlevel 1 goto :BAD
%LJLINK% /out:buildvm.exe buildvm*.obj
@if errorlevel 1 goto :BAD
if exist buildvm.exe.manifest^
  %LJMT% -manifest buildvm.exe.manifest -outputresource:buildvm.exe

buildvm -m elfasm -o lj_vm.s
@if errorlevel 1 goto :BAD
buildvm -m bcdef -o lj_bcdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
buildvm -m ffdef -o lj_ffdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
buildvm -m libdef -o lj_libdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
buildvm -m recdef -o lj_recdef.h %ALL_LIB%
@if errorlevel 1 goto :BAD
buildvm -m vmdef -o jit\vmdef.lua %ALL_LIB%
@if errorlevel 1 goto :BAD
buildvm -m folddef -o lj_folddef.h lj_opt_fold.c
@if errorlevel 1 goto :BAD

@rem ---- Cross compiler ----
@set OPTIONS= -fno-common -fno-short-enums -ffunction-sections -fdata-sections -fPIC -mcpu=cortex-a57+fp+simd+crypto+crc -fno-omit-frame-pointer
@set LJCOMPILE="%NINTENDO_SDK_ROOT%\Compilers\NX\nx\aarch64\bin\clang" -Wall %OPTIONS% -I%NINTENDO_SDK_ROOT%\Include -DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI -DLUAJIT_USE_SYSMALLOC -DLJ_TARGET_NX -DLJ_NO_SYSTEM -DNN_NINTENDO_SDK %GC64% -c
@set LJLIB="%NINTENDO_SDK_ROOT%\Compilers\NX\nx\aarch64\bin\aarch64-nintendo-nx-elf-ar" rc
@set INCLUDE=""

%NINTENDO_SDK_ROOT%\Compilers\NX\nx\aarch64\bin\aarch64-nintendo-nx-elf-as -o lj_vm.o lj_vm.s

@if "%1" neq "debug" goto :NODEBUG
@shift
@set LJCOMPILE=%LJCOMPILE% -DNN_SDK_BUILD_DEBUG -g -O0
@set TARGETLIB=libluajit-5.1-d.a
goto :BUILD
:NODEBUG
@set LJCOMPILE=%LJCOMPILE% -DNN_SDK_BUILD_RELEASE -O3
@set TARGETLIB=libluajit-5.1.a
:BUILD
del %TARGETLIB%
@if "%1" neq "noamalg" goto :AMALG
for %%f in (lj_*.c lib_*.c) do (
  %LJCOMPILE% %%f
  @if errorlevel 1 goto :BAD
)

%LJLIB% %TARGETLIB% lj_*.o lib_*.o
@if errorlevel 1 goto :BAD
@goto :NOAMALG
:AMALG
%LJCOMPILE% ljamalg.c
@if errorlevel 1 goto :BAD
%LJLIB% %TARGETLIB% ljamalg.o lj_vm.o
@if errorlevel 1 goto :BAD
:NOAMALG

@del *.o *.obj *.manifest minilua.exe buildvm.exe
@echo.
@echo === Successfully built LuaJIT for Nintendo Switch ===

@goto :END
:BAD
@echo.
@echo *******************************************************
@echo *** Build FAILED -- Please check the error messages ***
@echo *******************************************************
@goto :END
:FAIL
@echo To run this script you must open a "Visual Studio .NET Command Prompt"
@echo (64 bit host compiler). The Nintendo Switch NX SDK must be installed, too.
:END
